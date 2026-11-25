local c = require("common")
local BASE = c.BASE
local genres = c.genres
local util = c.util
local MAX_RESULT_PAGES = c.MAX_RESULT_PAGES
local http_req = c.http_req
return function()
  return { ok = true, data = genres.names }
end
