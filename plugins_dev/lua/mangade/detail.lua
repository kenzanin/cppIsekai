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
  if id == "" then return { ok = true, data = {} } end
  local d, err = get_json("/comics/" .. host.url_encode(id) .. "/view")
  if not d then return { ok = false, error = err } end
  return { ok = true, data = {
    id = tostring(d.id or id),
    name = clean(d.name),
    cover = d.image or "",
    description = clean(d.description),
    status = host.text.normalize_status(clean(d.status)),
    tags = (type(d.genre_names) == "string" and split_names(d.genre_names)) or names(d.genres),
    genres = names(d.genres),
    release_at = epoch(d.published_date),
    url = SITE .. "/comic/" .. tostring(d.slug or "") .. "-pid" .. tostring(d.id or id),
  } }
end
