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
return function()
  local data = get_json("/genres")
  return { ok = true, data = data and names(data.items) or {} }
end
