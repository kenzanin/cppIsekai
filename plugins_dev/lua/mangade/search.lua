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
  local q = clean(params.title or params.query or params.q)
  local page = math.max(1, tonumber(params.page) or 1)
  local path = "/comics?page=" .. page .. "&size=20&sort=newest"
  if q ~= "" then path = path .. "&name=" .. host.url_encode(q) end
  local ids = genre_ids(params.tags or params.genres)
  for i, id in ipairs(ids) do path = path .. "&genres%5B" .. (i - 1) .. "%5D=" .. host.url_encode(id) end
  local data, err = get_json(path)
  if not data then return { ok = false, error = err } end

  local items = {}
  for _, it in ipairs(data.list or {}) do
    if it.id then
      items[#items + 1] = {
        id = tostring(it.id),
        name = clean(it.name),
        cover = it.image or "",
        description = clean(it.description),
        url = SITE .. "/comic/" .. tostring(it.slug or "") .. "-pid" .. tostring(it.id),
      }
    end
  end
  return { ok = true, data = {
    items = items,
    total = tonumber(data.totalCount) or #items,
    pages = tonumber(data.totalPage) or 1,
  } }
end
