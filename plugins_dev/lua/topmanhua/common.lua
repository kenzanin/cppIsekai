-- TopManhua plugin (www.topmanhua.fan): Madara-family theme, Cloudflare-fronted.
-- Port of goIsekai examples/plugins/lua/topmanhua; host.http.fetch replaces
-- get_body/post_body (headers ride in opts), host.re2 replaces host.regex with
-- (pattern, subject) argument order. Envelope { ok, data | error }.
local BASE = "https://www.topmanhua.fan/"
-- Trailing slash is load-bearing: image CDNs reject "https://www.topmanhua.fan"
-- (403) but accept ".../fan/" (200); it is also the fallback Referer.
-- Covers: the search JSON carries only a relative path, but img-r1 served every
-- tested thumb across both base_cdn values while img-r2 503'd some — one host
-- for all (manifest param "cdn", inlined).
local CDN_COVER = "https://img-r1.2xstorage.com/"

-- Search is a JSON endpoint; replicate the browser's XHR headers. The
-- X-CSRF-Token the browser attaches is NOT enforced — a live probe without it
-- returned the identical payload — so no meta-tag round trip is needed.
local XHR = { ["X-Requested-With"] = "XMLHttpRequest", ["Referer"] = BASE .. "manhua" }

local function http_req(url, extra)
  local opts = { headers = {} }
  if extra then
    for k, v in pairs(extra) do
      opts.headers[k] = v
    end
  end
  return host.http.fetch_body(url, opts)
end

-- first capture group of the first match; nil when no match (host re2.capture).
local function find1(s, pattern)
  return host.re2.capture(pattern, tostring(s or ""))
end

local function search_json(query)
  return http_req(BASE .. "manga/searching?action=wp-manga-search-manga&title=" .. host.url_encode(query), XHR)
end

local function cover_from(r)
  if not r.image_original or r.image_original == "" then
    return ""
  end
  return CDN_COVER .. r.image_original
end

-- The detail page renders no author block at all; the search JSON carries the
-- author per slug, so one slug-matched lookup recovers it. Any failure
-- degrades to "" (host enrichment fills it when available).
local function lookup_author(slug, title)
  if title == "" then
    return ""
  end
  local body = search_json(title)
  if not body then
    return ""
  end
  local ok, decoded = pcall(json.decode, body)
  if not ok or type(decoded) ~= "table" then
    return ""
  end
  for _, r in ipairs(decoded.data or {}) do
    if r.slug == slug then
      return host.text.unescape(r.author or "")
    end
  end
  return ""
end

-- Genre archives are server-rendered with page-listing-item cards.
local function parse_genre_cards(body)
  local items, seen = {}, {}
  for _, m in
    ipairs(host.re2.find_all([[(?s)<div[^>]*class="[^"]*page-listing-item[^"]*".*?post-title.*?</h[23]>]], body))
  do
    local card = m[1]
    local slug = find1(card, [[href="[^"]*/manhua/([a-z0-9-]+)"]])
    if slug and slug ~= "" and not seen[slug] then
      seen[slug] = true
      local title = find1(card, [[(?s)<h[23][^>]*>\s*<a[^>]*>(.*?)</a>]])
      local cover = find1(card, [[data-src="[\t\n\s]*(https?://[^"]+)"]])
        or find1(card, [[src="[\t\n\s]*(https?://[^"]+)"]])
      items[#items + 1] = {
        id = slug,
        name = host.text.trim(host.text.unescape(host.text.strip_html(title or ""))),
        cover = cover and host.text.trim(cover) or "",
      }
    end
  end
  return items
end

-- ── search ─────────────────────────────────────────────────────────────────
-- ── genres ─────────────────────────────────────────────────────────────────

local genre_cache = nil -- { list = {{name,slug}}, by_name = {[name]=slug} }

-- global (not local): methods.search above references it before its
-- definition point, which would otherwise compile to a nil global lookup
function genres_scrape()
  if genre_cache then
    return genre_cache
  end
  local list, seen, by_name = {}, {}, {}
  local body, err = http_req(BASE .. "manhua")
  if body then
    for _, m in ipairs(host.re2.find_all([[<a href="/manhua-genre/([a-z0-9_-]+)"[^>]*>([^<]+)</a>]], body)) do
      local slug, label = m[1], host.text.unescape(host.text.trim(m[2]))
      if slug ~= "" and label ~= "" and not seen[slug] then
        seen[slug] = true
        list[#list + 1] = { name = label, slug = slug }
        by_name[label] = slug
      end
    end
  end
  genre_cache = { list = list, by_name = by_name }
  return genre_cache
end

return {
  BASE = BASE,
  CDN_COVER = CDN_COVER,
  XHR = XHR,
  http_req = http_req,
  find1 = find1,
  search_json = search_json,
  cover_from = cover_from,
  lookup_author = lookup_author,
  parse_genre_cards = parse_genre_cards,
  genre_cache = genre_cache,
  genres_scrape = genres_scrape,
}
