local c = require("common")
local BASE = c.BASE
local util = c.util
local genres = c.genres
local http_body = c.http_body
local total_memo = c.total_memo
local id_of = c.id_of
local pager_pages = c.pager_pages
local remember = c.remember
return function(params)
  local chapter_id = id_of(params)
  if chapter_id == "" then
    return { ok = true, data = {} }
  end
  local urls = util.get_page_urls(http_body, chapter_id:gsub(":", "/"))
  if not urls then
    return { ok = true, data = {} }
  end
  return { ok = true, data = urls }
end
