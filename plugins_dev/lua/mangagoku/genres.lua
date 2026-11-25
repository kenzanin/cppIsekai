local c = require("common")
local SITE = c.SITE
local OPTS = c.OPTS
local http_req = c.http_req
local get_json = c.get_json
local find1 = c.find1
local clean = c.clean
local epoch = c.epoch
local slug_of = c.slug_of
local search_page = c.search_page
local page_memo = c.page_memo
local paging = c.paging
local book_genres = c.book_genres
local book_description = c.book_description
return function()
  -- No standalone genre endpoint: read the site-wide catalogue from the
  -- schema.org Book blob of the first search hit.
  local first = search_page("isekai", 1)
  if type(first) ~= "table" or not first[1] or not first[1].slug then
    return { ok = true, data = {} }
  end
  local body = http_req("/manga/" .. host.url_encode(first[1].slug)) or ""
  return { ok = true, data = book_genres(body) }
end
