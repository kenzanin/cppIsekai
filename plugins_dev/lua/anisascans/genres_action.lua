local c = require("common")
local BASE = c.BASE
local trim = c.trim
local unescape = c.unescape
local strip_html = c.strip_html
local normalize_status = c.normalize_status
local http_req = c.http_req
local chapter_num = c.chapter_num
local find_all = c.find_all
local find1 = c.find1
return function()
  local names = require("genres").names
  return { ok = true, data = names }
end
