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
  local q = tostring(params.title or params.query or params.q or "")
  local page = math.max(1, tonumber(params.page) or 1)
  local tags = params.tags or params.genres or {}
  if type(tags) == "string" then -- UI joins checked genres with commas
    local list = {}
    for name in tags:gmatch("[^,]+") do list[#list + 1] = name:match("^%s*(.-)%s*$") end
    tags = list
  end

  local body
  if type(tags) == "table" and #tags > 0 then
    -- Genre browse: /ComicList/g=<id>/page/<N>/. First genre wins; the id is
    -- the DLE genre number (genres map resolves UI names, digits pass through).
    local g = tostring(tags[1] or "")
    local gid = (genre_map and genre_map[g]) or g:match("(%d+)")
    if not gid then return { ok = true, data = { items = {}, total = 0 } } end
    local url = "/ComicList/g=" .. tostring(gid)
    if page > 1 then url = url .. "/page/" .. tostring(page) .. "/" end
    body = site_get(url .. "/")
  elseif q ~= "" then
    local url = "/search/" .. host.url_encode(q)
    if page > 1 then url = url .. "/page/" .. tostring(page) end
    body = site_get(url)
  else
    return { ok = true, data = { items = {}, total = 0 } }
  end

  local items = parse_cd_cards(body or "")
  return { ok = true, data = { items = items, total = #items } }
end
