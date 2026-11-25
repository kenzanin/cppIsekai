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
return function(params)
  local manga_id = tostring(decode_id(params.id) or "")
  if manga_id == "" then
    return { ok = false, error = "comico: missing id" }
  end
  local data = http_get(API .. "/manga/" .. host.url_encode(manga_id))
  if not data then
    -- keep the id so the host can retry, as in goIsekai
    return { ok = true, data = {
      id = manga_id, name = "", cover = "", status = "unknown", tags = {},
      release_at = 0, description = "", artists = {},
      authors = {}, url = SITE .. "/manga/" .. manga_id,
    } }
  end

  local author
  for _, a in ipairs(data.authors or {}) do
    if type(a) == "table" and a.name then author = a.name break
    elseif type(a) == "string" then author = a break end
  end

  return { ok = true, data = {
    id = tostring(data.id or manga_id),
    name = trim(data.title or ""),
    cover = abs_url(data.coverUrl),
    status = host.text.normalize_status(data.status or ""),
    tags = tags_to_names(data.tags),
    -- no machine date on the manga endpoint
    release_at = 0,
    description = data.description or "",
    authors = author and { author } or {},
    artists = {},
    url = SITE .. "/manga/" .. manga_id,
  } }
end
