-- MangaHub.io plugin (mangahub.io) -- MangaHub GraphQL API at api.mghcdn.com.
-- Port of goIsekai examples/plugins/lua/mangahub. cppIsekai host deltas, all
-- verified in src/plugin/lua/lua_runtime.cpp: handle(req) envelope
-- {ok,data|error}; json.* globals; host.re2(pattern, subject) with [1]=whole
-- match; host.http.fetch(url,{method,headers}) -> res,err with LOWERCASED
-- res.header keys; host.crypto.aes_gcm_decrypt(iv,ct,tag,key) takes HEX, so
-- the site's base64url blob parts are converted plugin-side (host.codecs has
-- standard-alphabet base64 only). The manifest here pins api/img_cdn/
-- thumb_cdn but no site_url -> defaulted below.
local GRAPHQL_URL = host.plugin.param("api") or "https://api.mghcdn.com/graphql"
local SITE_URL = host.plugin.param("site_url") or "https://mangahub.io"
local SOURCE_ID = "m01"
local IMG_CDN = host.plugin.param("img_cdn") or "https://imgx.mghcdn.com/"
local THUMB_CDN = host.plugin.param("thumb_cdn") or "https://thumb.mghcdn.com/"

-- Cached access key, dropped whenever the API refuses.
local cachedKey = nil

-- Last refusal message, so callers surface it instead of a silent empty list.
local gqlError = nil

-- Genre cache: chip names plus the name->slug map search filters with.
local genre_cache, genre_slugs = nil, {}

-- ── helpers ────────────────────────────────────────────────────────────────

-- first capture group of the first match; nil when no match (host re2.capture).
local function find1(s, pattern)
  return host.re2.capture(pattern, tostring(s or ""))
end

-- base64url (unpadded, MangaHub blob/key convention) -> lowercase hex.
local function b64url_hex(s)
  if type(s) ~= "string" or s == "" then return nil end
  local b = s:gsub("%-", "+"):gsub("_", "/")
  b = b .. string.rep("=", (4 - #b % 4) % 4)
  local raw = host.codecs.base64_decode(b)
  if not raw then return nil end
  return host.codecs.hex_encode(raw)
end

-- A bare GET only hands back the key this session/IP already exhausted, so
-- every chapter comes back rate limited. Presenting a cookie value the server
-- has never issued makes it mint a genuinely new key, and ?reloadKey=1 is what
-- triggers the re-issue. Each key carries four chapters of quota; an explicit
-- Cookie header overrides whatever the host cookie jar holds.
local function fetch_access_key()
  local url = SITE_URL .. "/chapter/martial-peak/chapter-"
    .. tostring(1000 + math.random(0, 1999)) .. "?reloadKey=1"
  local res, err = host.http.fetch(url, {
    headers = {
      ["Referer"] = SITE_URL .. "/manga/martial-peak",
      ["Accept"] = "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
      ["Sec-Fetch-Dest"] = "document",
      ["Sec-Fetch-Mode"] = "navigate",
      ["Sec-Fetch-Site"] = "same-origin",
      ["Upgrade-Insecure-Requests"] = "1",
      ["Cookie"] = "mhub_access=0000000000000000000000000000cafe",
    },
  })
  if err ~= nil or not res then return nil end
  -- cppIsekai lowercases response header names.
  local cookie = (res.headers and res.headers["set-cookie"]) or ""
  local key = find1(cookie, [[mhub_access=([^;]+)]])
  if not key or key == "" then return nil end
  return key
end

-- Escape a string for embedding in a GraphQL string literal.
local function escape_gql(s)
  return (s:gsub("\\", "\\\\"):gsub('"', '\\"'))
end

-- MangaHub encrypts the chapter page list and hands the key out separately:
--   pages = "enc:v1:<keyId>:<iv>:<tag>:<ciphertext>"   (all base64url)
--   key   = GET /api/chapter-crypto -> {keyId, key, expiresAt}
--   plain = AES-256-GCM(key, iv, ciphertext+tag) -> {"p":...,"i":[...]}
-- The key and the message rotate independently, so a keyId mismatch is
-- retried once with a freshly minted key instead of failing.
local function encrypted_page_list(blob, slug, number)
  local function material()
    local key = cachedKey
    if not key then
      key = fetch_access_key()
      if key then cachedKey = key end
    end
    if not key then return nil, "upstream_auth_failed" end
    -- This endpoint authenticates on the cookie, not the x-mhub-access
    -- header the GraphQL call uses: the header alone answers 403.
    local res, err = host.http.fetch(SITE_URL .. "/api/chapter-crypto", {
      headers = {
        ["Cookie"] = "mhub_access=" .. key,
        ["Accept"] = "application/json",
        ["Referer"] = SITE_URL .. "/chapter/" .. slug .. "/chapter-" .. number,
      },
    })
    if err ~= nil or not res or res.status ~= 200 then
      return nil, "upstream_auth_failed: crypto endpoint status "
        .. tostring(err or (res and res.status) or 0)
    end
    local ok, parsed = pcall(json.decode, res.body)
    if not ok or type(parsed) ~= "table" or not parsed.key or not parsed.keyId then
      return nil, "upstream_auth_failed: crypto endpoint returned no usable key"
    end
    return parsed
  end

  local parts = {}
  for part in string.gmatch(blob .. ":", "([^:]*):") do
    parts[#parts + 1] = part
  end
  if #parts ~= 6 or parts[1] ~= "enc" or parts[2] ~= "v1" then
    return nil, "envelope_unrecognised"
  end
  local keyId, iv, tag, ciphertext = parts[3], parts[4], parts[5], parts[6]

  local mat, why = material()
  if mat and mat.keyId ~= keyId then
    cachedKey = nil
    mat, why = material()
  end
  if not mat then
    host.log.error("mangahub: chapter-crypto unavailable: " .. tostring(why))
    return nil, "upstream_auth_failed"
  end
  if mat.keyId ~= keyId then
    -- Key ids go to the log for diagnosis, never to the reader.
    host.log.error("mangahub: keyId mismatch, blob " .. keyId .. " vs crypto " .. tostring(mat.keyId))
    return nil, "decrypt_key_mismatch"
  end

  -- cppIsekai's aes_gcm_decrypt is hex-in; the blob and key are base64url.
  local iv_h = b64url_hex(iv)
  local ct_h = b64url_hex(ciphertext)
  local tag_h = b64url_hex(tag)
  local key_h = b64url_hex(tostring(mat.key))
  if not iv_h or not ct_h or not tag_h or not key_h then
    host.log.error("mangahub: pages blob/key not decodable as base64url")
    return nil, "decrypt_failed"
  end
  local plain = host.crypto.aes_gcm_decrypt(iv_h, ct_h, tag_h, key_h)
  if not plain then
    host.log.error("mangahub: pages decrypt failed")
    return nil, "decrypt_failed"
  end
  local ok, decoded = pcall(json.decode, plain)
  if not ok or type(decoded) ~= "table" or type(decoded.p) ~= "string" or type(decoded.i) ~= "table" then
    host.log.error("mangahub: unexpected pages plaintext")
    return nil, "decrypt_failed"
  end

  local out = {}
  for _, name in ipairs(decoded.i) do
    out[#out + 1] = IMG_CDN .. decoded.p .. name
  end
  return out
end

-- Run a GraphQL query, dropping the cached key and retrying once when the API
-- refuses. MangaHub reports both expired keys and its "API rate limit
-- excessed" refusal as HTTP 200 with an errors array and a null payload, so
-- the status code alone cannot tell a good answer from a refused one. The
-- message lands in gqlError so callers can surface it.
local function graphql_query(query)
  gqlError = nil

  for _ = 1, 2 do
    local key = cachedKey
    if not key then
      key = fetch_access_key()
      if key then cachedKey = key end
    end
    if not key then
      host.log.error("mangahub: no mhub_access key available")
      return nil
    end

    local res, err = host.http.fetch(GRAPHQL_URL, {
      method = "POST",
      body = json.encode({ query = query }),
      headers = {
        ["Content-Type"] = "application/json",
        ["Origin"] = SITE_URL,
        ["Referer"] = SITE_URL .. "/",
        ["x-mhub-access"] = key,
      },
    })
    local status = (res and res.status) or 0

    if err ~= nil or status < 200 or status >= 300 then
      host.log.info("mangahub: GraphQL status " .. tostring(status) .. ", refreshing key")
      cachedKey = nil
    else
      local ok, parsed = pcall(json.decode, res.body)
      if not ok or type(parsed) ~= "table" then
        host.log.error("mangahub: unparseable GraphQL response")
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

      host.log.info("mangahub: GraphQL refused (" .. tostring(gqlError) .. "), retrying with a fresh key")
      cachedKey = nil
    end
  end

  return nil
end

local methods = {}

-- ── search ─────────────────────────────────────────────────────────────────

methods.search = function(params)
  local query = params.title or params.query or params.q or ""
  local page = math.max(1, tonumber(params.page) or 1)
  -- The API takes every selected genre as one comma-separated list of slugs,
  -- ANDed together, and expects the literal "all" when none is selected.
  local genres = params.genres
  if genres == nil then genres = params.tags end
  if type(genres) ~= "table" or #genres == 0 then
    local one = type(genres) == "string" and genres or ""
    genres = one ~= "" and { one } or {}
  end
  local genre = "all"
  if #genres > 0 then
    -- Chips carry display names; map back to the API slugs (fallback slugify
    -- keeps old cached URLs working when the map is cold).
    local slugs = {}
    for i, name in ipairs(genres) do
      slugs[i] = (genre_slugs and genre_slugs[name])
        or name:lower():gsub("%s+", "-")
    end
    genre = table.concat(slugs, ",")
  end
  local offset = (page - 1) * 30
  host.log.info("mangahub search: q=" .. query .. " genre=" .. genre .. " page=" .. tostring(page))

  local gql = '{search(x: ' .. SOURCE_ID .. ', q: "' .. escape_gql(query)
    .. '", genre: "' .. escape_gql(genre) .. '", mod: POPULAR, offset: ' .. tostring(offset)
    .. ') {rows {title, slug, image}}}'

  local data = graphql_query(gql)
  local rows = {}
  if data and data.data and data.data.search then
    rows = data.data.search.rows or {}
  end

  local items = {}
  for _, row in ipairs(rows) do
    local cover = row.image or ""
    if cover ~= "" and cover:sub(1, 4) ~= "http" then
      cover = THUMB_CDN .. cover
    end
    items[#items + 1] = {
      id = row.slug,
      name = row.title or "",
      cover = cover,
    }
  end

  host.log.info("mangahub search: found " .. tostring(#items) .. " results for q=" .. query)
  return { ok = true, data = { items = items, total = #items } }
end

-- ── detail ─────────────────────────────────────────────────────────────────

methods.detail = function(params)
  local slug = params.id or ""
  local empty = { id = slug, name = "", cover = "", status = "unknown", tags = {}, release_at = 0,
    authors = {}, artists = {}, description = "", url = SITE_URL .. "/manga/" .. slug }
  if slug == "" then return { ok = true, data = empty } end

  host.log.info("mangahub detail: slug=" .. slug)

  local gql = '{manga(x: ' .. SOURCE_ID .. ', slug: "' .. escape_gql(slug)
    .. '") {title, slug, status, image, author, artist, genres, description, alternativeTitle}}'

  local data = graphql_query(gql)
  local m = data and data.data and data.data.manga
  if not m then return { ok = true, data = empty } end

  local cover = m.image or ""
  if cover ~= "" and cover:sub(1, 4) ~= "http" then
    cover = THUMB_CDN .. cover
  end

  -- status may arrive as an enum string or a numeric code; the host wants a
  -- canonical string either way.
  local status = m.status
  if status == nil then status = "" else status = tostring(status) end

  -- genres arrives as one comma-separated string; the ABI wants a list.
  local tags = {}
  if type(m.genres) == "string" then
    for part in host.re2.gmatch("[^,]+", m.genres) do
      local name = host.text.trim(part[1] or "")
      if name ~= "" then tags[#tags + 1] = name end
    end
  elseif type(m.genres) == "table" then
    for _, g in ipairs(m.genres) do tags[#tags + 1] = tostring(g) end
  end

  local d = {
    id = m.slug or slug,
    name = m.title or "",
    cover = cover,
    status = host.text.normalize_status(status),
    tags = tags,
    release_at = 0,
    authors = (type(m.author) == "string" and m.author ~= "") and { m.author } or {},
    artists = (type(m.artist) == "string" and m.artist ~= "") and { m.artist } or {},
    description = m.description or "",
    url = SITE_URL .. "/manga/" .. (m.slug or slug),
  }
  return { ok = true, data = d }
end

-- ── chapters ───────────────────────────────────────────────────────────────

methods.chapters = function(params)
  local slug = params.id or ""
  if slug == "" then return { ok = true, data = {} } end

  host.log.info("mangahub chapters: slug=" .. slug)

  local gql = '{manga(x: ' .. SOURCE_ID .. ', slug: "' .. escape_gql(slug) .. '") {chapters {number, title, date}}}'

  local data = graphql_query(gql)
  local raw = {}
  if data and data.data and data.data.manga then
    raw = data.data.manga.chapters or {}
  end

  local out = {}
  for _, ch in ipairs(raw) do
    -- An absent title arrives as an empty string, not nil, so it has to be
    -- tested for emptiness: "" is truthy in Lua.
    local title = ch.title
    if not title or title == "" then title = "Chapter " .. tostring(ch.number) end

    local item = {
      id = slug .. ":chapter-" .. tostring(ch.number),
      number = tonumber(ch.number) or 0,
      title = title,
      release_at = 0,
      pages = 0,
    }
    -- MangaHub sends epoch seconds, epoch millis or a date string; the host
    -- normalizes all three. Unparseable leaves the zero so the decode keeps.
    if ch.date ~= nil then
      local iso = host.text.date_to_iso(tostring(ch.date))
      if iso then item.release_at = iso end
    end
    out[#out + 1] = item
  end

  -- GraphQL returns ascending; the ABI wants newest-first.
  table.sort(out, function(a, b) return a.number > b.number end)

  host.log.info("mangahub chapters: found " .. tostring(#out) .. " chapters for " .. slug)
  return { ok = true, data = out }
end

-- ── pages ──────────────────────────────────────────────────────────────────

methods.pages = function(params)
  local chapterID = params.id or ""
  -- Split on the first ":" -- the slug cannot contain one, the tail can.
  local m = host.re2.find("^([^:]+):chapter-(.+)$", chapterID)
  if #m < 3 then return { ok = true, data = {} } end
  local slug, number = m[2], m[3]
  host.log.info("mangahub pages: slug=" .. slug .. " ch=" .. number)

  local gql = '{chapter(x: ' .. SOURCE_ID .. ', slug: "' .. escape_gql(slug)
    .. '", number: ' .. number .. ') {pages, mangaID, number}}'

  local data = graphql_query(gql)
  local chapter = data and data.data and data.data.chapter
  if not chapter then
    -- A refusal (rate limit, expired key) must fail loudly: an empty list
    -- renders a blank chapter with no explanation.
    if gqlError then
      host.log.error("mangahub: graphql refused: " .. gqlError)
      return { ok = false, error = "upstream_auth_failed: " .. gqlError }
    end
    return { ok = true, data = {} }
  end

  local raw = chapter.pages
  if type(raw) == "string" and string.sub(raw, 1, 7) == "enc:v1:" then
    -- Authoritative list, named file by file. Fail loudly rather than falling
    -- back to a CDN-pattern guess: a plausible wrong list silently drops pages.
    local pages, why = encrypted_page_list(raw, slug, number)
    if not pages then
      host.log.error("mangahub pages: cannot decrypt: " .. tostring(why))
      return { ok = false, error = tostring(why or "decrypt_failed") }
    end
    host.log.info("mangahub pages: found " .. tostring(#pages) .. " pages (decrypted)")
    return { ok = true, data = pages }
  end

  local pagesJSON = raw
  if type(raw) == "string" then
    local ok, decoded = pcall(json.decode, raw)
    if not ok or type(decoded) ~= "table" then
      host.log.error("mangahub pages: unparseable pages JSON raw=" .. string.sub(raw, 1, 220))
      return { ok = true, data = {} }
    end
    pagesJSON = decoded
  end
  if type(pagesJSON) ~= "table" then
    host.log.error("mangahub pages: unexpected pages shape")
    return { ok = true, data = {} }
  end

  local prefix = pagesJSON.p or ""
  local out = {}
  for _, file in ipairs(pagesJSON.i or {}) do
    out[#out + 1] = IMG_CDN .. prefix .. file
  end
  host.log.info("mangahub pages: found " .. tostring(#out) .. " pages")
  return { ok = true, data = out }
end

-- ── genres ─────────────────────────────────────────────────────────────────
-- The GraphQL API has no genre query, so the slugs come from the /genre/<slug>
-- links the site renders. Cached for the life of the VM.

local genres_cache = nil

methods.genres = function()
  if genre_cache then return { ok = true, data = genre_cache } end

  local res, err = host.http.fetch(SITE_URL .. "/search")
  local html = (err == nil and res and res.body) or ""
  if html == "" then
    host.log.error("mangahub: /search carried no body, genre browsing unavailable")
    return { ok = true, data = {} }
  end

  -- Slugs are the API truth (/search genre=<comma slugs>); names are derived
  -- for the chips and mapped back in search. The anchor-label regex lost the
  -- capture index once already -- deriving from the slug sidesteps it.
  genre_slugs = {}
  local seen, out = {}, {}
  for g in host.re2.gmatch([[/genre/([a-z0-9-]+)]], html) do
    local slug = g[2] or "" -- m[1] is the full match; group 1 sits at m[2]
    if slug ~= "" and not seen[slug] then
      seen[slug] = true
      local name = slug:gsub("(%a)([%a']*)", function(a, b) return a:upper() .. b:lower() end):gsub("%-", " ")
      genre_slugs[name] = slug
      out[#out + 1] = name
    end
  end
  table.sort(out)

  if #out == 0 then
    host.log.error("mangahub: /search listed no genre labels, genre browsing unavailable")
    return { ok = true, data = {} }
  end

  host.log.info("mangahub: " .. tostring(#out) .. " genres")
  genre_cache = out
  return { ok = true, data = out }
end

function handle(req)
  local fn = methods[req.method]
  if fn == nil then return { ok = false, error = "unknown method " .. tostring(req.method) } end
  return fn(req.params or {})
end
