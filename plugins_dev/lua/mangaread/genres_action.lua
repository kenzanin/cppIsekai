local c = require("common")
local BASE = c.BASE
local GENRE_PATH = c.GENRE_PATH
local http_req = c.http_req
local find1 = c.find1
return function()
  return { ok = true, data = require("genres").names }
end
