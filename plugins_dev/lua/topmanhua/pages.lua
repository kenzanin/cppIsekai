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
  local composite = params.id or ""
  local slug, cslug = composite:match("^([^:]+):(.+)$")
  if not slug or not cslug then
    return { ok = false, error = "topmanhua: chapter id must be slug:chapterSlug, got " .. composite }
  end

  local body, err = http_req(BASE .. "manhua/" .. host.url_encode(slug) .. "/" .. host.url_encode(cslug))
  if err ~= nil then return { ok = false, error = err } end

  -- attributes are single-quoted on this site; src == data-src and data-backup
  -- points at the second CDN. The images also need Referer: BASE (site root) —
  -- a per-image-header contract does not exist in cppIsekai's page list, so
  -- the reader's own referer handling covers it (host-gap, see report).
  local out = {}
  local srcs = host.html.attr(body, "img.wp-manga-chapter-img", "src")
  if #srcs == 0 then srcs = host.html.attr(body, "img.wp-manga-chapter-img", "data-src") end
  if #srcs == 0 then srcs = host.html.attr(body, "img.wp-manga-chapter-img", "data-backup") end
  for _, s in ipairs(srcs) do
    local url = host.text.trim(s)
    if url ~= "" then out[#out + 1] = url end
  end
  if #out == 0 then return { ok = false, error = "topmanhua: no pages for " .. composite } end
  return { ok = true, data = out }
end
