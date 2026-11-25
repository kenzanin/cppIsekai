local c = require("common")
local BASE = c.BASE
local opts = c.opts
local http_req = c.http_req
local find1 = c.find1
local slug_of = c.slug_of
local url_for = c.url_for
local chapter_url = c.chapter_url
local page_urls = c.page_urls
return function()
  return { ok = true, data = require("genres").names }
end
