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
return function()
  local form = get("/advanced-search") or ""
  local _, names = categories(form)
  return { ok = true, data = names }
end
