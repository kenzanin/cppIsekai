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
return function()
  if cached_genres then
    return { ok = true, data = cached_genres }
  end
  local names = {}
  local data = http_get(API .. "/tags?source=comick-metadata")
  if type(data) == "table" then
    for _, t in ipairs(data) do
      if type(t) == "table" and t.name then
        names[#names + 1] = tostring(t.name)
      end
    end
    table.sort(names)
  end
  cached_genres = names
  return { ok = true, data = names }
end
