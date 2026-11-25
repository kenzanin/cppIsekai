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
return function()
  local body = site_get("/comix/")
  local f = dle.json_data(body, "__XFILTER__ = ")
  local values = f and f.filter_items and f.filter_items.g and f.filter_items.g.values or nil
  local names, map = {}, {}
  if type(values) == "table" then
    for _, v in ipairs(values) do
      if type(v.value) == "string" and v.value ~= "" then
        names[#names + 1] = v.value
        map[v.value] = tostring(v.id)
      end
    end
  end
  genre_map = map
  table.sort(names)
  return { ok = true, data = names }
end
