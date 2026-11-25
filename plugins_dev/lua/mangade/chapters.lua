local c = require("common")
local API = c.API
local SITE = c.SITE
local OPTS = c.OPTS
local get_json = c.get_json
local find1 = c.find1
local clean = c.clean
local epoch = c.epoch
local split_names = c.split_names
local names = c.names
local comic_id = c.comic_id
local genre_ids = c.genre_ids
return function(params)
  local id = comic_id(params.id)
  if id == "" then
    return { ok = true, data = {} }
  end
  local d, err = get_json("/comics/" .. host.url_encode(id) .. "/view")
  if not d then
    return { ok = false, error = err }
  end
  local out = {}
  for _, c in ipairs(d.news_chapters or {}) do
    if c.id then
      out[#out + 1] = {
        id = id .. ":" .. tostring(c.id),
        number = tonumber(c.chapter_number) or 0,
        title = clean(c.name),
        release_at = epoch(c.published_date),
        pages = 0,
        url = SITE .. "/comic/" .. tostring(d.slug or "") .. "-pid" .. id .. "/" .. tostring(c.slug or ""),
      }
    end
  end
  table.sort(out, function(a, b)
    return a.number < b.number
  end)
  return { ok = true, data = out }
end
