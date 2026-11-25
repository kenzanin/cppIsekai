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
  local id = slug_of(params.id or "")
  if id == "" then return { ok = true, data = {} } end
  local doc, err = get_json("/api/manga/" .. host.url_encode(id) .. "/chapters?limit=-1")
  if not doc then return { ok = false, error = err } end
  local out = {}
  for _, c in ipairs((doc.data or {}).chapters or {}) do
    if c.chapter_slug and c.chapter_slug ~= "" then
      out[#out + 1] = {
        id = id .. ":" .. c.chapter_slug,
        number = tonumber(c.chapter_num) or 0,
        title = clean(c.chapter_name or "Chapter"),
        release_at = epoch(c.updated_at),
        pages = 0,
      }
    end
  end
  table.sort(out, function(a, b) return a.number < b.number end)
  return { ok = true, data = out }
end
