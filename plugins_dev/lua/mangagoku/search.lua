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
  local q = clean(params.title or params.query or params.q or "")
  local page = math.max(1, tonumber(params.page) or 1)
  if q == "" then
    return { ok = true, data = { items = {}, total = 0, pages = 1 } }
  end
  local items_doc = search_page(q, page)
  if not items_doc then
    return { ok = false, error = "mangagoku: search failed" }
  end
  local items = {}
  for _, it in ipairs(items_doc) do
    if it.slug and it.slug ~= "" then
      items[#items + 1] = {
        id = it.slug,
        name = clean(it.name),
        cover = it.thumb or "",
        author = clean(it.author or ""),
      }
    end
  end
  local total, pages = paging(q)
  return { ok = true, data = { items = items, total = total, pages = pages } }
end
