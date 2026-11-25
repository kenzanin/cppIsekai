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
  local manga_id = slug_of(params.id or "")
  if manga_id == "" then return { ok = true, data = {} } end

  local body, err = http_req(url_for("manga", manga_id))
  if err ~= nil then return { ok = false, error = err } end

  local selector = "div.chapters table tbody tr td div.chapter a"
  local urls = host.html.attr(body, selector, "href")
  local titles = host.html.select(body, selector)

  -- The table is newest-first already, which is the ABI convention: keep order.
  local out = {}
  for i, href in ipairs(urls) do
    -- .../c247, .../c230.5 and .../c0-v2 all end in the segment to keep.
    local tail = find1(href, [[/([^/]+)$]]) or href
    out[#out + 1] = {
      id = manga_id .. ":" .. tail,
      -- The number is the tail of the href: .../c247, .../c230.5, .../c0-v2
      number = tonumber(find1(href, [[/c([\d.]+)]])) or 0,
      title = host.text.trim(titles[i] or ""),
      release_at = 0, -- page shows no machine dates
      pages = 0,
    }
  end
  return { ok = true, data = out }
end
