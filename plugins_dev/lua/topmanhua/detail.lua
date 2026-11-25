local c = require("common")
local BASE = c.BASE
local CDN_COVER = c.CDN_COVER
local XHR = c.XHR
local http_req = c.http_req
local find1 = c.find1
local search_json = c.search_json
local cover_from = c.cover_from
local lookup_author = c.lookup_author
local parse_genre_cards = c.parse_genre_cards
local genres_scrape = c.genres_scrape
return function(params)
  local slug = host.text.trim(tostring(params.id or ""))
  local empty = {
    id = slug,
    name = "",
    cover = "",
    status = "unknown",
    tags = {},
    release_at = 0,
    description = "",
    authors = {},
    artists = {},
    url = BASE .. "manhua/" .. host.url_encode(slug),
  }
  if slug == "" then
    return { ok = true, data = empty }
  end

  local body, err = http_req(BASE .. "manhua/" .. host.url_encode(slug))
  if err ~= nil then
    return { ok = false, error = err }
  end

  local data = empty
  data.name = host.text.unescape(host.text.trim(find1(body, [[(?s)<div class="post-title">\s*<h1>(.*?)</h1>]]) or ""))

  local img = find1(body, [[(?s)<div class="summary_image".*?</a>]]) or ""
  data.cover = host.text.trim(
    find1(img, [[data-src="[\t\n\s]*(https?://[^"]+)"]]) or find1(img, [[src="[\t\n\s]*(https?://[^"]+)"]]) or ""
  )

  local gblock = find1(body, [[(?s)class="genres-content">(.*?)</div>]]) or ""
  local tags = {}
  for _, g in ipairs(host.re2.find_all([[<a[^>]*>([^<]+)</a>]], gblock)) do
    local name = host.text.unescape(host.text.trim(g[2] or ""))
    if name ~= "" then
      tags[#tags + 1] = name
    end
  end
  data.tags = tags

  local s = find1(body, [[(?s)Status\s*</h5>.*?class="summary-content">\s*([^<]+)]])
  data.status = s and host.text.normalize_status(host.text.trim(s)) or "unknown"

  -- .summary__content paragraphs; the first <p> just repeats the title here.
  -- trim runs after unescape so a bare &nbsp; paragraph collapses to "".
  local dblock = find1(body, [[(?s)class="summary__content[^"]*">(.*?)</div>]]) or ""
  local paras = {}
  for _, p in ipairs(host.re2.find_all([[(?s)<p[^>]*>(.*?)</p>]], dblock)) do
    local t = host.text.trim(host.text.unescape(host.text.strip_html(p[1])):gsub("%s+", " "))
    if t ~= "" and t ~= data.name then
      paras[#paras + 1] = t
    end
  end
  data.description = table.concat(paras, "\n")

  local author = lookup_author(slug, data.name)
  if author ~= "" then
    data.authors = { author }
  end
  return { ok = true, data = data }
end

-- ── chapters ───────────────────────────────────────────────────────────────
