local c = require("common")
local BASE = c.BASE
local OPTS = c.OPTS
local http_req = c.http_req
local find1 = c.find1
local clean = c.clean
local first = c.first
local get_json = c.get_json
local json_url = c.json_url
local split_id = c.split_id
local tags_of = c.tags_of
return function(params)
  local dir, permalink = split_id(params.id)
  if not dir or not permalink then return { ok = true, data = {} } end
  local doc, err = get_json(json_url(dir, permalink))
  if not doc then return { ok = false, error = err } end
  local status = table.concat(tags_of(doc.tags, "Status"), ", ")
  return { ok = true, data = {
    id = dir .. ":" .. permalink,
    name = clean(doc.name or ""),
    cover = doc.cover and host.url_abs(BASE, clean(doc.cover)) or "",
    description = clean(doc.description or ""),
    status = host.text.normalize_status(status),
    tags = tags_of(doc.tags, "General"),
    authors = tags_of(doc.tags, "Author"),
    artists = {},
    alt_titles = doc.aliases or {},
    url = doc.link or (BASE .. "/" .. dir .. "/" .. permalink),
  } }
end
