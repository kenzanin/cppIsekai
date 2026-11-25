local c = require("common")
local SITE = c.SITE
local API = c.API
local UA = c.UA
local HEADERS = c.HEADERS
local trim = c.trim
local abs_url = c.abs_url
local abs_url = c.abs_url
local tags_to_names = c.tags_to_names
local decode_id = c.decode_id
local http_get = c.http_get
local http_post_json = c.http_post_json
local chapter_pages = c.chapter_pages
return function(params)
  local chapter_id = tostring(decode_id(params.id) or "")
  if chapter_id == "" then
    return { ok = false, error = "comico: missing id" }
  end

  local access = http_get(API .. "/reader/access/" .. host.url_encode(chapter_id))
  if not access or not access.token then
    return { ok = false, error = "comico: no access token for " .. chapter_id }
  end

  local body = http_post_json(API .. "/reader/chapters/" .. host.url_encode(chapter_id)
    .. "/open?d=" .. host.url_encode(tostring(access.token)), '{"proxyMethod":"all"}')
  if not body then
    return { ok = false, error = "comico: reader open failed for " .. chapter_id }
  end

  local ok, opened = pcall(json.decode, body)
  if not ok or type(opened) ~= "table" then
    return { ok = false, error = "comico: bad reader payload for " .. chapter_id }
  end

  local out = {}
  for _, p in ipairs(opened.pages or {}) do
    out[#out + 1] = abs_url(p)
  end
  if #out == 0 then
    return { ok = false, error = "comico: no pages for " .. chapter_id }
  end
  return { ok = true, data = out }
end
