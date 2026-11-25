local c = require("common")
local BASE = c.BASE
local trim = c.trim
local http_req = c.http_req
local find_all = c.find_all
local epoch_ms_to_secs = c.epoch_ms_to_secs
local per_page = c.per_page
local page_cache = c.page_cache
local fetch_search = c.fetch_search
local page_info = c.page_info
return function()
  return { ok = true, data = require("genres").names }
end
