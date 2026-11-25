local c = require("common")
local BASE = c.BASE
local util = c.util
local genres = c.genres
local http_body = c.http_body
local total_memo = c.total_memo
local id_of = c.id_of
local pager_pages = c.pager_pages
local remember = c.remember
return function()
  return { ok = true, data = genres.names }
end
