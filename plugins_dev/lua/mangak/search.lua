local c = require("common")
local API = c.API
local SITE = c.SITE
local OPTS = c.OPTS
local get_json = c.get_json
local find1 = c.find1
local names = c.names
local epoch = c.epoch
local ensure_genres = c.ensure_genres
local slug_of = c.slug_of
return function(params)
  local q = host.text.trim(tostring(params.title or params.query or params.q or ""))
  local page = math.max(1, tonumber(params.page) or 1)
  local limit = math.max(1, math.min(60, tonumber(params.limit) or 24))
  local tags = params.tags or params.genres or {}
  if type(tags) == "string" then
    local list = {}
    for name in tags:gmatch("[^,]+") do list[#list + 1] = host.text.trim(name) end
    tags = list
  end
  local path = "/titles/search?q=" .. host.url_encode(q) .. "&limit=" .. limit .. "&page=" .. page
  if type(tags) == "table" and #tags > 0 then
    local slug = ensure_genres()[tags[1]] or tags[1]
    path = path .. "&genres=" .. host.url_encode(slug)
  end
  local data, err = get_json(path)
  if not data then return { ok = false, error = err } end

  local items = {}
  for _, it in ipairs(data.items or {}) do
    if it.id and it.id ~= "" then
      items[#items + 1] = {
        id = it.id,
        name = host.text.unescape(it.name or ""),
        cover = it.cover or "",
        status = host.text.normalize_status(it.status or ""),
        url = SITE .. (it.url or ("/manga/" .. it.id)),
      }
    end
  end
  local pg = data.pagination or {}
  return { ok = true, data = { items = items, total = tonumber(pg.total) or #items,
                               pages = tonumber(pg.total_pages) or 1 } }
end
