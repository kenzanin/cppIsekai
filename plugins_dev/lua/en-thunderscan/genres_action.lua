local c = require("common")
local BASE = c.BASE
local trim = c.trim
local cap = c.cap
local http_body = c.http_body
local fetch_detail_body = c.fetch_detail_body
return function()
  return { ok = true, data = require("genres").names }
end
