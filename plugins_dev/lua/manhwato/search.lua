local c = require("common")
local SITE = c.SITE
local get = c.get
local find1 = c.find1
local find_all = c.find_all
local clean = c.clean
local epoch = c.epoch
local cards = c.cards
local last_page = c.last_page
local genre_slug = c.genre_slug
local page_cache = c.page_cache
local page_info = c.page_info
return function(params)
  local q = clean(params.title or params.query or params.q)
  local page = math.max(1, tonumber(params.page) or 1)
  local tags = params.tags or params.genres
  local key = q:lower()
  local path
  if type(tags) == "table" and #tags > 0 then
    local slug = genre_slug(tostring(tags[1]))
    if not slug then return { ok = true, data = { items = {}, total = 0, pages = 1 } } end
    path = "/manhwa-genre/" .. slug
    key = key .. "|" .. slug
  elseif type(tags) == "string" and tags ~= "" then
    local slug = genre_slug(tags)
    if not slug then return { ok = true, data = { items = {}, total = 0, pages = 1 } } end
    path = "/manhwa-genre/" .. slug
    key = key .. "|" .. slug
  else
    path = "/search?q=" .. host.url_encode(q)
  end
  local body, err = get(path .. (page > 1 and ("&page=" .. page) or ""))
  if not body then return { ok = false, error = err or "manhwato: search failed" } end
  local items = cards(body)
  local info = page_info(key, path, body, #items)
  return { ok = true, data = { items = items, total = info.total, pages = info.pages } }
end
