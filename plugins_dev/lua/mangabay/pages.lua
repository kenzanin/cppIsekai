local c = require("common")
local BASE = c.BASE
local dle = c.dle
local find1 = c.find1
local num_str = c.num_str
local site_get = c.site_get
local abs_url = c.abs_url
local path_of = c.path_of
local manga_id_of = c.manga_id_of
local parse_cd_cards = c.parse_cd_cards
local normalize_status = c.normalize_status
local genre_map = c.genre_map
return function(params)
  local reader = tostring(params.id or "")
  if reader == "" then
    return { ok = true, data = {} }
  end
  -- chapters rows cached before the num_str fix carry the float ids
  -- (/reader/15840.0/991476.0); normalize so those sessions still resolve.
  reader = reader:gsub("%.0$", ""):gsub("%.0/", "/")
  local body = site_get(path_of(reader))
  local data = dle.json_data(body, "window.__DATA__ = ")
  local images = data and type(data.images) == "table" and data.images or {}
  local count = tonumber(data and data.pages) or #images
  if count > #images then
    count = #images
  end
  local out = {}
  for i = 1, count do
    if type(images[i]) == "string" and images[i] ~= "" then
      out[#out + 1] = images[i]
    end
  end
  return { ok = true, data = out }
end
