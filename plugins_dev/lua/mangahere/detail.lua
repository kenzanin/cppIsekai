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
  local id = id_of(params)
  if id == "" then
    return { ok = true, data = {} }
  end
  local html = http_body(BASE .. "/manga/" .. id .. "/")
  if not html or html == "" then
    return { ok = false, error = "mangahere: detail fetch failed" }
  end
  return { ok = true, data = util.parse_manga_detail(html, id) }
end
