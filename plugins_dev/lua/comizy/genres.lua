local c = require("common")
local API = c.API
local WEB = c.WEB
local get_json = c.get_json
local cached_build_id = c.cached_build_id
local cached_genres = c.cached_genres
local name_to_slug = c.name_to_slug
local build_id = c.build_id
local split_id = c.split_id
local comizy_chapter_num = c.comizy_chapter_num
local items_from = c.items_from
return function()
  if cached_genres then return { ok = true, data = cached_genres } end
  local names = {}
  local res = get_json(API .. "/genres")
  if res then
    for _, g in ipairs(((res.data or {}).items) or {}) do
      if g.slug and g.name then
        names[#names + 1] = tostring(g.name)
        name_to_slug[string.lower(tostring(g.name))] = tostring(g.slug)
      end
    end
    table.sort(names)
  end
  cached_genres = names
  return { ok = true, data = names }
end
