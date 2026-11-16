-- 1Manga plugin (1manga.co): MangaHub GraphQL API at api.mghcdn.com, source mn03.
-- Port of goIsekai examples/plugins/lua/1manga; adapted to cppIsekai host tools
-- (host.http.fetch replaces host.http.get/post; host.re2.* replaces host.regex.*;
-- host.crypto.aes_gcm_decrypt is hex-in/raw-out, so base64url fields are
-- converted via host.codecs first).
-- Envelope { ok, data | error }; canonical manga fields per the mangadex plugin.
local SITE = "https://1manga.co"
local GRAPHQL = "https://api.mghcdn.com/graphql"
local IMG_CDN = "https://imgx.mghcdn.com/"
local THUMB_CDN = "https://thumb.mghcdn.com/"
local SOURCE_ID = "mn03"

-- Cached access key, dropped whenever the API refuses.
local cachedKey = nil

-- Last refusal message, so callers surface it instead of a silent empty list.
local gqlError = nil

-- ── helpers ────────────────────────────────────────────────────────────────

-- http request -> response table {status, body, headers}, or nil, err.
local function http_req(url, opts)
  local res, err = host.http.fetch(url, opts or {})
  if err ~= nil then return nil, err end
  if res.status ~= 200 then return nil, "1manga: upstream status " .. tostring(res.status) end
  return res, nil
end

-- A bare GET only hands back the key this session/IP already exhausted, so
-- every chapter comes back rate limited. Presenting a cookie value the server
-- has never issued makes it mint a genuinely new key, and ?reloadKey=1 is what
-- triggers the re-issue. Each key so obtained carries four chapters of quota.
-- cppIsekai header keys are lowercased; the jar is opt-in via cookie=true.
local function fetch_access_key()
  local url = SITE .. "/chapter/martial-peak/chapter-" .. tostring(1000 + math.random(0, 1999)) .. "?reloadKey=1"
  local res, err = host.http.fetch(url, {
    cookie = true,
    headers = {
      ["Referer"] = SITE .. "/manga/martial-peak",
      ["Accept"] = "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
      ["Cookie"] = "mhub_access=0000000000000000000000000000cafe",
    },
  })
  if err ~= nil or not res.headers then return nil end

  local cookie = res.headers["set-cookie"] or ""
  local m = host.re2.find([[mhub_access=([^;]+)]], cookie)
  if #m < 2 or m[2] == "" then return nil end
  return m[2]
end

-- Escape a string for embedding in a GraphQL string literal.
local function escape_gql(s)
  return (s:gsub("\\", "\\\\"):gsub('"', '\\"'))
end

-- MangaHub encrypts the chapter's page list and hands the key out separately:
--   pages  = "enc:v1:<keyId>:<iv>:<tag>:<ciphertext>"   (all base64url)
--   key    = GET /api/chapter-crypto -> {keyId, key, expiresAt}
--   plain  = AES-256-GCM(key, iv, ciphertext+tag) -> {"p":"<prefix>","i":["1a.jpg",...]}
-- Decryption runs in the host (host.crypto, hex-in/raw-out); the base64url
-- fields are normalized and hex-encoded plugin-side first.
local function b64url_to_hex(s)
  local std = s:gsub("%-", "+"):gsub("_", "/")
  local raw = host.codecs.base64_decode(std)
  if not raw then return nil end
  return host.codecs.hex_encode(raw)
end

local function encrypted_page_list(blob)
  local function material()
    local key = cachedKey
    if not key then
      key = fetch_access_key()
      if key then cachedKey = key end
    end
    if not key then return nil, "upstream_auth_failed" end
    -- This endpoint authenticates on the cookie, not the x-mhub-access
    -- header the GraphQL call uses: the header alone answers 403.
    local res, err = host.http.fetch(SITE .. "/api/chapter-crypto", {
      cookie = true,
      headers = {
        ["Cookie"] = "mhub_access=" .. key,
        ["Accept"] = "application/json",
      },
    })
    if err ~= nil or res.status ~= 200 then
      return nil, "upstream_auth_failed: crypto endpoint status " .. tostring(err or res.status)
    end
    local ok, parsed = pcall(json.decode, res.body)
    if not ok or type(parsed) ~= "table" or not parsed.key or not parsed.keyId then
      return nil, "upstream_auth_failed: crypto endpoint returned no usable key"
    end
    return parsed
  end

  local parts = {}
  for part in string.gmatch(blob .. ":", "([^:]*):") do parts[#parts + 1] = part end
  if #parts ~= 6 or parts[1] ~= "enc" or parts[2] ~= "v1" then
    host.log.error("1manga: unrecognised pages envelope")
    return nil, "envelope_unrecognised"
  end
  local _, _, keyId, iv, tag, ciphertext = parts[1], parts[2], parts[3], parts[4], parts[5], parts[6]

  local mat, why = material()
  if mat and mat.keyId ~= keyId then
    -- The key is served per session, so a stale cookie yields a key the
    -- message was not encrypted under. Drop it and ask again once.
    cachedKey = nil
    mat, why = material()
  end
  if not mat then
    host.log.error("1manga: chapter-crypto unavailable: " .. tostring(why))
    return nil, "upstream_auth_failed"
  end
  if mat.keyId ~= keyId then
    host.log.error("1manga: keyId mismatch, blob " .. keyId .. " vs crypto " .. tostring(mat.keyId))
    return nil, "decrypt_key_mismatch"
  end

  local iv_hex, tag_hex, ct_hex, key_hex = b64url_to_hex(iv), b64url_to_hex(tag), b64url_to_hex(ciphertext),
    b64url_to_hex(mat.key)
  if not iv_hex or not tag_hex or not ct_hex or not key_hex then
    host.log.error("1manga: pages fields not decodable as base64")
    return nil, "decrypt_failed"
  end
  -- host.crypto order is (iv, ct, tag, key); plaintext comes back raw.
  local plain = host.crypto.aes_gcm_decrypt(iv_hex, ct_hex, tag_hex, key_hex)
  if not plain then
    host.log.error("1manga: pages decrypt failed")
    return nil, "decrypt_failed"
  end
  local ok, decoded = pcall(json.decode, plain)
  if not ok or type(decoded) ~= "table" or type(decoded.p) ~= "string" or type(decoded.i) ~= "table" then
    host.log.error("1manga: unexpected pages plaintext")
    return nil, "decrypt_failed"
  end

  local out = {}
  for idx, name in ipairs(decoded.i) do
    out[idx] = { index = idx - 1, url = IMG_CDN .. decoded.p .. name }
  end
  return out
end

-- Run a GraphQL query, dropping the cached key and retrying once when the API
-- refuses. MangaHub reports both expired keys and its "API rate limit
-- excessed" refusal as HTTP 200 with an errors array and a null payload, so
-- the host status alone cannot tell a good answer from a refused one. The
-- message is kept in gqlError so callers can surface it.
local function graphql_query(query)
  gqlError = nil

  for _ = 1, 2 do
    local key = cachedKey
    if not key then
      key = fetch_access_key()
      if key then cachedKey = key end
    end
    if not key then
      host.log.error("1manga: no mhub_access key available")
      return nil
    end

    local res, err = http_req(GRAPHQL, {
      method = "POST",
      body = json.encode({ query = query }),
      headers = {
        ["Content-Type"] = "application/json",
        ["Origin"] = SITE,
        ["Referer"] = SITE .. "/",
        ["x-mhub-access"] = key,
      },
    })
    if err ~= nil then
      host.log.info("1manga: GraphQL transport error, refreshing key: " .. tostring(err))
      cachedKey = nil
    else
      local ok, parsed = pcall(json.decode, res.body)
      if not ok or type(parsed) ~= "table" then
        host.log.error("1manga: unparseable GraphQL response")
        return nil
      end

      local errors = parsed.errors
      if errors and #errors > 0 then
        gqlError = errors[1].message or "GraphQL error"
      elseif parsed.data == nil then
        gqlError = "GraphQL response carried no data"
      else
        return parsed
      end

      host.log.info("1manga: GraphQL refused (" .. tostring(gqlError) .. "), retrying with a fresh key")
      cachedKey = nil
    end
  end

  return nil
end

local function cover_url(image)
  if image and image ~= "" and image:sub(1, 4) ~= "http" then return THUMB_CDN .. image end
  return image or ""
end

local methods = {}

-- ── search ─────────────────────────────────────────────────────────────────

methods.search = function(params)
  local query = params.title or params.query or params.q or ""
  local page = math.max(1, tonumber(params.page) or 1)
  local offset = (page - 1) * 30

  -- GraphQL takes one genre name per call; multi-genre means intersecting
  -- result sets, which one query cannot express. First genre wins.
  local genres = params.tags or params.genres or {}
  local genre = "all"
  if type(genres) == "table" and #genres > 0 then genre = genres[1] end

  local gql = "{search(x: " .. SOURCE_ID .. ', q: "' .. escape_gql(query) .. '", genre: "'
    .. escape_gql(genre) .. '", mod: POPULAR, offset: ' .. tostring(offset) .. ") {rows {title, slug, image}}}"

  local data = graphql_query(gql)
  local rows = {}
  if data and data.data and data.data.search then rows = data.data.search.rows or {} end

  local items = {}
  for _, row in ipairs(rows) do
    items[#items + 1] = { id = row.slug, name = row.title or "", cover = cover_url(row.image) }
  end
  return { ok = true, data = { items = items, total = #items } }
end

-- ── detail ─────────────────────────────────────────────────────────────────

methods.detail = function(params)
  local slug = params.id or ""
  local empty = { id = slug, name = "", cover = "", status = "unknown", tags = {}, release_at = 0,
    description = "", authors = {}, artists = {}, url = SITE .. "/manga/" .. slug }
  if slug == "" then return { ok = true, data = empty } end

  local gql = "{manga(x: " .. SOURCE_ID .. ', slug: "' .. escape_gql(slug)
    .. '") {title, slug, status, image, author, artist, genres, description, alternativeTitle}}'
  local data = graphql_query(gql)
  local m = data and data.data and data.data.manga
  if not m then
    if gqlError then return { ok = false, error = "1manga: " .. gqlError } end
    return { ok = true, data = empty }
  end

  -- genres arrives as one comma-separated string; the canonical shape wants a list.
  local tags = {}
  if type(m.genres) == "string" then
    for part in string.gmatch(m.genres .. ",", "([^,]+)") do
      local name = host.text.trim(part)
      if name ~= "" then tags[#tags + 1] = name end
    end
  elseif type(m.genres) == "table" then
    tags = m.genres
  end

  local data_out = empty
  data_out.name = m.title or ""
  data_out.cover = cover_url(m.image)
  -- status may arrive as an enum string or a numeric code; unknown passes
  -- through the host normalizer either way.
  data_out.status = host.text.normalize_status(m.status and tostring(m.status) or "")
  data_out.tags = tags
  data_out.description = m.description or ""
  if m.author and m.author ~= "" then data_out.authors = { m.author } end
  if m.artist and m.artist ~= "" then data_out.artists = { m.artist } end
  return { ok = true, data = data_out }
end

-- ── chapters ───────────────────────────────────────────────────────────────

methods.chapters = function(params)
  local slug = params.id or ""
  if slug == "" then return { ok = true, data = {} } end

  local gql = "{manga(x: " .. SOURCE_ID .. ', slug: "' .. escape_gql(slug) .. '") {chapters {number, title, date}}}'
  local data = graphql_query(gql)
  local raw = {}
  if data and data.data and data.data.manga then raw = data.data.manga.chapters or {} end

  local out = {}
  for _, ch in ipairs(raw) do
    -- An absent title arrives as an empty string, not nil.
    local title = ch.title
    if not title or title == "" then title = "Chapter " .. tostring(ch.number) end
    local item = {
      id = slug .. ":chapter-" .. tostring(ch.number),
      number = host.text.chapter_num(tostring(ch.number)) or 0,
      title = title,
      release_at = 0, -- MangaHub dates arrive in mixed shapes; host.date_to_iso on the raw value below
      pages = 0,
    }
    -- MangaHub sends epoch seconds, epoch millis or a date string; the host
    -- normalizes all three. Absent/nil keeps the field out.
    if ch.date ~= nil then
      local iso = host.text.date_to_iso(tostring(ch.date))
      if iso then item.release_at = iso end
    end
    out[#out + 1] = item
  end

  -- GraphQL returns ascending; canonical order is newest-first.
  table.sort(out, function(a, b) return a.number > b.number end)
  return { ok = true, data = out }
end

-- ── pages ──────────────────────────────────────────────────────────────────

methods.pages = function(params)
  local composite = params.id or ""
  local slug, number = composite:match("^([^:]+):chapter-(.+)$")
  if not slug or not number then
    return { ok = false, error = "1manga: chapter id must be slug:chapter-N, got " .. tostring(params.id) }
  end

  local gql = "{chapter(x: " .. SOURCE_ID .. ', slug: "' .. escape_gql(slug) .. '", number: ' .. number
    .. ") {pages, mangaID, number}}"
  local data = graphql_query(gql)
  local chapter = data and data.data and data.data.chapter
  if not chapter then
    -- A refusal (rate limit, expired key) must fail loudly: returning an
    -- empty list renders a blank chapter with no explanation.
    if gqlError then return { ok = false, error = "1manga: " .. gqlError } end
    return { ok = false, error = "1manga: no chapter data for " .. composite }
  end

  -- Authoritative page list: the API hands back the exact file names, which
  -- no CDN probe can do (this manga names pages "1a.jpg 2b.jpg ..."). Two
  -- payload shapes:
  --   "enc:v1:<keyId>:<iv>:<tag>:<ciphertext>" -> AES-256-GCM, see above, or
  --   {"p":"<prefix>/","i":["1a.jpg",...]}     -> plaintext JSON.
  local pages_field = chapter.pages
  if type(pages_field) == "string" and string.sub(pages_field, 1, 7) == "enc:v1:" then
    local pages, why = encrypted_page_list(pages_field)
    if pages then return { ok = true, data = pages } end
    -- Fail loudly instead of probing: an encrypted field means the probe
    -- pattern is already known to be wrong for this chapter.
    return { ok = false, error = "1manga: " .. tostring(why or "decrypt_failed") }
  end
  if type(pages_field) == "string" and pages_field ~= "" then
    local ok, decoded = pcall(json.decode, pages_field)
    if ok and type(decoded) == "table" and type(decoded.p) == "string" and type(decoded.i) == "table" then
      local pages = {}
      for idx, name in ipairs(decoded.i) do
        pages[#pages + 1] = IMG_CDN .. decoded.p .. name
      end
      return { ok = true, data = pages }
    end
    host.log.info("1manga: unrecognised pages payload, falling back to probe")
  end

  -- Fallback for a missing or unparseable pages field: find the page count by
  -- probing (page N exists with 200, N+1 404s), binary search, ~9 requests.
  -- The extension varies per chapter, so probe candidates once on page 1.
  local ext
  for _, c in ipairs({ "jpg", "jpeg", "png", "webp" }) do
    local res, err = host.http.fetch(IMG_CDN .. slug .. "/" .. number .. "/1." .. c,
      { headers = { Referer = SITE .. "/" } })
    if err == nil and res.status == 200 then ext = c break end
  end
  if not ext then
    -- No usable page 1: letter-suffixed files with no readable payload.
    return { ok = false, error = "1manga: no usable page 1 for " .. composite }
  end
  local function page_exists(i)
    local res, err = host.http.fetch(IMG_CDN .. slug .. "/" .. number .. "/" .. i .. "." .. ext,
      { headers = { Referer = SITE .. "/" } })
    return err == nil and res.status == 200
  end
  local lo, hi = 1, 200
  while lo < hi do
    local mid = math.ceil((lo + hi + 1) / 2)
    if page_exists(mid) then lo = mid else hi = mid - 1 end
  end
  local pages = {}
  for i = 1, lo do
    pages[#pages + 1] = IMG_CDN .. slug .. "/" .. number .. "/" .. i .. "." .. ext
  end
  return { ok = true, data = pages }
end

-- ── genres ─────────────────────────────────────────────────────────────────

methods.genres = function()
  return { ok = true, data = require("genres").names }
end

function handle(req)
  local fn = methods[req.method]
  if fn == nil then return { ok = false, error = "unknown method " .. tostring(req.method) } end
  return fn(req.params or {})
end
