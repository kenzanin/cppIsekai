local c = require("common")
local BASE = c.BASE
local helpers = c.helpers
local genres = c.genres
local find1 = c.find1
local bare_slug = c.bare_slug
local label_value = c.label_value
local epoch = c.epoch
return function()
  return { ok = true, data = genres.names }
end
