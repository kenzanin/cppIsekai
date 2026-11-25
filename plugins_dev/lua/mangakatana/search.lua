local c = require("common")
local BASE = c.BASE
local opts = c.opts
local http_req = c.http_req
local find1 = c.find1
local slug_of = c.slug_of
local url_for = c.url_for
local chapter_url = c.chapter_url
local page_urls = c.page_urls
return function(params)
  local q = params.title or params.query or params.q or ""
  local page = math.max(1, tonumber(params.page) or 1)
  local genres = params.tags or params.genres or {}
  if type(genres) == "string" then
    local list = {}
    for name in genres:gmatch("[^,]+") do list[#list + 1] = host.text.trim(name) end
    genres = list
  end

  local url
  if type(genres) == "table" and #genres > 0 then
    -- Genre browsing: /genre/<slug>; page 2+ moves the path to /page/N. The
    -- archive reuses the search card layout, so one parser serves both.
    local slug = require("genres").map[genres[1]] or genres[1]
    url = BASE .. "/genre/" .. host.url_encode(slug)
    if page > 1 then url = url .. "/page/" .. page end
  else
    -- Page 1 is the bare query string; later pages /page/N?...
    url = BASE .. "/?search=" .. host.url_encode(q) .. "&search_by=book_name"
    if page > 1 then
      url = BASE .. "/page/" .. page .. "?search=" .. host.url_encode(q) .. "&search_by=book_name"
    end
  end

  local body, err = http_req(url)
  if err ~= nil then return { ok = false, error = err } end

  -- Every card carries a title link and one cover <img> (results without a
  -- cover use /imgs/no-cover.png), so the lists stay index-aligned.
  local hrefs = host.html.attr(body, "#book_list .item h3.title a", "href")
  local titles = host.html.select(body, "#book_list .item h3.title a")
  local covers = host.html.attr(body, "#book_list .item .media img", "src")

  local items, seen = {}, {}
  for i, href in ipairs(hrefs) do
    local slug = slug_of(href)
    if slug and not seen[slug] then
      seen[slug] = true
      items[#items + 1] = { id = slug, name = host.text.unescape(titles[i] or ""), cover = covers[i] or "" }
    end
  end
  return { ok = true, data = { items = items, total = #items } }
end
