-- Page-list scraper via the site's same-origin GraphQL.
-- The chapter HTML server-renders only the first six <img> tags; the full
-- list exists solely behind POST /graphql. 1manga.co is a MangaHub
-- white-label (source enum mn03), so the mangahub.io flow applies verbatim:
-- mint a mhub_access cookie (?reloadKey=1 with a junk cookie), query
-- chapter{pages}, and expand the {p:prefix, i:[names]} blob over the img CDN.
-- The enc:v1 encrypted branch is kept from the mangahub port in case the
-- payload rotates; the HTML scrape this replaces embedded 6 of N pages.
local IMG_CDN = host.plugin.param("img_cdn") or "https://imgx.mghcdn.com/"
local SOURCE = "mn03"

-- Cached access key, dropped whenever the API refuses.
local cachedKey = nil

-- Last refusal message, so failures surface instead of a silent empty list.
local gqlError = nil

-- base64url (unpadded, MangaHub blob/key convention) -> lowercase hex.
local function b64url_hex(s)
  if type(s) ~= "string" or s == "" then
    return nil
  end
  local b = s:gsub("%-", "+"):gsub("_", "/")
  b = b .. string.rep("=", (4 - #b % 4) % 4)
  local raw = host.codecs.base64_decode(b)
  if not raw then
    return nil
  end
  return host.codecs.hex_encode(raw)
end

-- A bare GET only hands back the key this session/IP already exhausted, so
-- every chapter comes back rate limited. Presenting a cookie value the server
-- has never issued makes it mint a genuinely new key, and ?reloadKey=1 is
-- what triggers the re-issue (mhub_access in the response headers).
local function fetch_access_key(ctx, slug, chid)
  local res, err = host.http.fetch(ctx.BASE .. "/chapter/" .. slug .. "/" .. chid .. "?reloadKey=1", {
    headers = {
      ["Referer"] = ctx.BASE .. "/manga/" .. slug,
      ["Accept"] = "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
      ["Cookie"] = "mhub_access=0000000000000000000000000000cafe",
    },
  })
  if err ~= nil or not res then
    return nil
  end
  local cookie = (res.headers and res.headers["set-cookie"]) or ""
  local key = host.re2.capture([[mhub_access=([^;]+)]], cookie)
  if not key or key == "" then
    return nil
  end
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
local function encrypted_page_list(ctx, blob, slug, chid)
  local function material()
    local key = cachedKey
    if not key then
      key = fetch_access_key(ctx, slug, chid)
      if key then
        cachedKey = key
      end
    end
    if not key then
      return nil, "upstream_auth_failed"
    end
    local res, err = host.http.fetch(ctx.BASE .. "/api/chapter-crypto", {
      headers = {
        ["Cookie"] = "mhub_access=" .. key,
        ["Accept"] = "application/json",
        ["Referer"] = ctx.BASE .. "/chapter/" .. slug .. "/" .. chid,
      },
    })
    if err ~= nil or not res or res.status ~= 200 then
      return nil, "upstream_auth_failed: crypto endpoint status " .. tostring(err or (res and res.status) or 0)
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
    return nil, tostring(why or "upstream_auth_failed")
  end
  if mat.keyId ~= keyId then
    return nil, "decrypt_key_mismatch"
  end

  local iv_h = b64url_hex(iv)
  local ct_h = b64url_hex(ciphertext)
  local tag_h = b64url_hex(tag)
  local key_h = b64url_hex(tostring(mat.key))
  if not iv_h or not ct_h or not tag_h or not key_h then
    return nil, "decrypt_failed"
  end
  local plain = host.crypto.aes_gcm_decrypt(iv_h, ct_h, tag_h, key_h)
  if not plain then
    return nil, "decrypt_failed"
  end
  local ok, decoded = pcall(json.decode, plain)
  if not ok or type(decoded) ~= "table" or type(decoded.p) ~= "string" or type(decoded.i) ~= "table" then
    return nil, "decrypt_failed"
  end

  local out = {}
  for _, name in ipairs(decoded.i) do
    out[#out + 1] = IMG_CDN .. decoded.p .. name
  end
  return out
end

-- Run the chapter query, dropping the cached key and retrying once when the
-- API refuses. MangaHub reports refusals as HTTP 200 with an errors array
-- and a null payload, so the status code alone cannot tell them apart.
-- Returns the parsed response, or nil with gqlError set.
local function graphql(ctx, gql, slug, chid)
  gqlError = nil

  for _ = 1, 2 do
    local key = cachedKey
    if not key then
      key = fetch_access_key(ctx, slug, chid)
      if key then
        cachedKey = key
      end
    end
    if not key then
      host.log.error("1manga: no mhub_access key available")
      gqlError = "no access key"
      return nil
    end

    local res, err = host.http.fetch(ctx.BASE .. "/graphql", {
      method = "POST",
      body = json.encode({ query = gql }),
      headers = {
        ["Content-Type"] = "application/json",
        ["Origin"] = ctx.BASE,
        ["Referer"] = ctx.BASE .. "/",
        ["Cookie"] = "mhub_access=" .. key,
        ["x-mhub-access"] = key,
      },
    })
    local status = (res and res.status) or 0

    if err ~= nil or status < 200 or status >= 300 then
      host.log.info("1manga: GraphQL status " .. tostring(status) .. ", refreshing key")
      cachedKey = nil
    else
      local ok, parsed = pcall(json.decode, res.body)
      if not ok or type(parsed) ~= "table" then
        host.log.error("1manga: unparseable GraphQL response")
        gqlError = "unparseable response"
        return nil
      end

      local errors = parsed.errors
      if errors and #errors > 0 then
        gqlError = errors[1].message or "GraphQL error"
      elseif parsed.data == nil or parsed.data.chapter == nil then
        gqlError = "GraphQL response carried no chapter"
      else
        return parsed.data.chapter
      end

      host.log.info("1manga: GraphQL refused (" .. tostring(gqlError) .. "), retrying with a fresh key")
      cachedKey = nil
    end
  end

  return nil
end

return function(ctx)
  return function(params)
    local composite = params.id or ""
    local slug, chid = string.match(composite, "^(.+):(chapter%-[%d%.]+)$")
    if not slug then
      return { ok = false, error = "1manga: chapter id must be slug:chapter-N, got " .. tostring(composite) }
    end
    -- The GraphQL number argument is numeric: normalize "07" -> 7, keep 12.5.
    local num = tostring(tonumber(string.match(chid, "chapter%-(.+)$")) or 0)

    local chapter = graphql(
      ctx,
      "{chapter(x: " .. SOURCE .. ', slug: "' .. escape_gql(slug) .. '", number: ' .. num .. ") {pages}}",
      slug,
      chid
    )
    if not chapter then
      -- A refusal must fail loudly: an empty list renders a blank chapter.
      if gqlError then
        host.log.error("1manga: graphql refused: " .. gqlError)
        return { ok = false, error = "upstream_auth_failed: " .. gqlError }
      end
      return { ok = false, error = "1manga: no pages for " .. composite }
    end

    local raw = chapter.pages
    if type(raw) == "string" and string.sub(raw, 1, 7) == "enc:v1:" then
      local pages, why = encrypted_page_list(ctx, raw, slug, chid)
      if not pages then
        host.log.error("1manga pages: cannot decrypt: " .. tostring(why))
        return { ok = false, error = tostring(why or "decrypt_failed") }
      end
      host.log.info("1manga: found " .. tostring(#pages) .. " pages for " .. composite .. " (decrypted)")
      return { ok = true, data = pages }
    end

    local pagesJSON = raw
    if type(raw) == "string" then
      local ok, decoded = pcall(json.decode, raw)
      if not ok or type(decoded) ~= "table" or type(decoded.i) ~= "table" then
        host.log.error("1manga pages: unparseable pages JSON raw=" .. string.sub(tostring(raw), 1, 220))
        return { ok = false, error = "1manga: unparseable pages for " .. composite }
      end
      pagesJSON = decoded
    end
    if type(pagesJSON) ~= "table" then
      host.log.error("1manga pages: unexpected pages shape")
      return { ok = false, error = "1manga: unexpected pages for " .. composite }
    end

    local prefix = pagesJSON.p or ""
    local out = {}
    for _, file in ipairs(pagesJSON.i or {}) do
      out[#out + 1] = IMG_CDN .. prefix .. file
    end
    if #out == 0 then
      return { ok = false, error = "1manga: no pages for " .. composite }
    end
    host.log.info("1manga: found " .. tostring(#out) .. " pages for " .. composite)
    return { ok = true, data = out }
  end
end
