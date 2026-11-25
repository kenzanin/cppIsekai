local c = require("common")
local BASE = c.BASE
local OPTS = c.OPTS
local http_req = c.http_req
local find1 = c.find1
local clean = c.clean
local first = c.first
local status_of = c.status_of
local series_url = c.series_url
return function(params)
  local slug = find1(params.id or "", [[^/([^/]+)]]) or tostring(params.id or "")
  local body, err = http_req(series_url(slug))
  if not body or body == "" then return { ok = false, error = err } end
  local by_text = clean(first(host.html.select(body, "p.l2-by")) or "")
  by_text = by_text:gsub("^By%s+", "")
  local author, artist = by_text:match("^([^,]+),(.+)$")
  author = clean(author or by_text)
  artist = clean(artist or "")
  local kicker = clean(first(host.html.select(body, "p.l2-kicker")) or "")
  if kicker == "" then kicker = clean(first(host.html.select(body, "div.l2-meta span")) or "") end
  local tags = {}
  for _, g in ipairs(host.html.select(body, "a[href*='tag%5B%5D']") or {}) do
    g = clean(g)
    if g ~= "" then tags[#tags + 1] = g end
  end
  local desc = clean(first(host.html.attr(body, "meta[property='og:description']", "content")) or "")
  return { ok = true, data = {
    id = slug,
    name = clean(first(host.html.select(body, "h1.l2-title")) or ""),
    cover = first(host.html.attr(body, "meta[property='og:image']", "content")) or "",
    description = desc,
    status = host.text.normalize_status(status_of(kicker)),
    tags = tags,
    authors = author ~= "" and { author } or {},
    artists = artist ~= "" and { artist } or {},
    url = series_url(slug),
  } }
end
