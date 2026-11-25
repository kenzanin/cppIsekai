local c = require("common")
local BASE = c.BASE
local OPTS = c.OPTS
local XHR = c.XHR
local http_req = c.http_req
local find1 = c.find1
local clean = c.clean
local first = c.first
local manga_url = c.manga_url
local slug_of = c.slug_of
local parse_cards = c.parse_cards
return function(params)
  local slug = slug_of(params.id or "")
  local body, err = http_req(manga_url(slug))
  if not body or body == "" then
    return { ok = false, error = err }
  end
  local tags, authors = {}, {}
  for _, g in ipairs(host.html.select(body, ".mt-comic-badges a, .mt-hero-genres a") or {}) do
    g = clean(g)
    if g ~= "" then
      tags[#tags + 1] = g
    end
  end
  for _, a in ipairs(host.html.select(body, ".mt-author-line a") or {}) do
    a = clean(a)
    if a ~= "" then
      authors[#authors + 1] = a
    end
  end
  local cover = first(host.html.attr(body, ".mt-comic-poster img", "data-src") or {})
    or first(host.html.attr(body, ".mt-comic-poster img", "src") or "")
    or ""
  return {
    ok = true,
    data = {
      id = slug,
      name = clean(first(host.html.select(body, "h1[itemprop=name]")) or ""),
      cover = cover,
      description = clean(first(host.html.select(body, "#synopsisModal p, .mt-comic-synopsis p")) or ""),
      status = host.text.normalize_status(clean(first(host.html.select(body, ".mt-status-tag")) or "")),
      tags = tags,
      authors = authors,
      artists = {},
      url = manga_url(slug),
    },
  }
end
