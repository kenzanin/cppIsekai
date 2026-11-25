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
return function(params)
  local composite = tostring(params.id or "")
  local id, chapter = composite:match("^([^:]+):(.+)$")
  if not id or not chapter then return { ok = true, data = {} } end
  local body, err = http_req("/manga/" .. host.url_encode(id) .. "/" .. host.url_encode(chapter))
  if not body or body == "" then return { ok = false, error = err } end
  -- Reader images are 2xstorage URLs with a trailing /<n>.webp index; the CDN
  -- host varies per title (img-r1/img-r2/imgs-2), so match any subdomain. Thumb
  -- images live under /thumb/ and never carry that numeric tail.
  local found = {}
  for url in body:gmatch([[https://[%w%-]+%.2xstorage%.com/[^"\\%s]+/%d+%.webp]]) do
    found[#found + 1] = url
  end
  -- The same page appears on several CDN mirrors; keep one URL per index.
  local by_index, order = {}, {}
  for _, url in ipairs(found) do
    local idx = tonumber(url:match('/([0-9]+)%.webp$'))
    if idx and not by_index[idx] then
      by_index[idx] = url
      order[#order + 1] = idx
    end
  end
  table.sort(order)
  local out = {}
  for _, idx in ipairs(order) do out[#out + 1] = by_index[idx] end
  return { ok = true, data = out }
end
