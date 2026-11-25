local c = require("common")
local BASE = c.BASE
local genres = c.genres
local BROWSER_UA = c.BROWSER_UA
local http_body = c.http_body
local date_epoch = c.date_epoch
return function()
  return { ok = true, data = genres.names }
end
