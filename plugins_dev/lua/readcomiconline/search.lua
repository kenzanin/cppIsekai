local c = require("common")
local BASE = c.BASE
local UA = c.UA
local COOKIE = c.COOKIE
local OPTS = c.OPTS
local get = c.get
local find1 = c.find1
local all = c.all
local clean = c.clean
local epoch = c.epoch
local categories = c.categories
local search_url = c.search_url
local parse_search = c.parse_search
return function(params)
  local q = clean(params.title or params.query or params.q or "")
  if q == "" then return { ok = true, data = { items = {}, total = 0, pages = 1 } } end
  local html, err = get(search_url(q, params.tags or params.genres))
  if not html then return { ok = false, error = "readcomiconline: search failed" .. (err and (": " .. err) or "") } end
  local items = parse_search(html)
  return { ok = true, data = { items = items, total = #items, pages = 1 } }
end
