local c = require("common")
local BASE = c.BASE
local UA = c.UA
local LIMIT = c.LIMIT
local STATUS = c.STATUS
local get = c.get
local txt = c.txt
local cards = c.cards
return function(params)
  local q = params.title or params.query or params.q or ""
  local page = tonumber(params.page) or 1
  local offset = (page - 1) * LIMIT
  local tag = params.tags or params.tag
  local args = "display_mode=Full+Display&author=&text=" .. host.url_encode(q) ..
    "&sort=Best+Match&order=Descending&official=Any&anime=Any&adult=Any&limit=" .. LIMIT .. "&offset=" .. offset
  if tag and tag ~= "" then
    for t in tostring(tag):gmatch("([^,]+)") do
      args = args .. "&included_tag=" .. host.url_encode(host.text.trim(t))
    end
  end
  local body, err = get("/search/data?" .. args)
  if err then return { ok = false, error = err } end
  local items = cards(body)
  return { ok = true, data = { items = items, total = #items } }
end
