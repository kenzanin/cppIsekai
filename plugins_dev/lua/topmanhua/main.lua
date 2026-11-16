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
    for k, v in pairs(extra) do opts.headers[k] = v end
  end
  local res, err = host.http.fetch(url, opts)
  if err ~= nil then return nil, err end
  if res.status ~= 200 then return nil, "topmanhua: upstream status " .. tostring(res.status) end
  return res.body, nil
end

-- first capture group of the first match; nil when the pattern does not match.
local function find1(s, pattern)
  local m = host.re2.find(pattern, tostring(s or ""))
  if #m >= 2 then return m[2] end
  if #m == 1 then return m[1] end
  return nil
end

local function search_json(query)
  return http_req(BASE .. "manga/searching?action=wp-manga-search-manga&title=" .. host.url_encode(query), XHR)
end

local function cover_from(r)
  if not r.image_original or r.image_original == "" then return "" end
  return CDN_COVER .. r.image_original
end

-- The detail page renders no author block at all; the search JSON carries the
-- author per slug, so one slug-matched lookup recovers it. Any failure
-- degrades to "" (host enrichment fills it when available).
local function lookup_author(slug, title)
  if title == "" then return "" end
  local body = search_json(title)
  if not body then return "" end
  local ok, decoded = pcall(json.decode, body)
  if not ok or type(decoded) ~= "table" then return "" end
  for _, r in ipairs(decoded.data or {}) do
    if r.slug == slug then return host.text.unescape(r.author or "") end
  end
  return ""
end

-- Genre archives are server-rendered with page-listing-item cards.
local function parse_genre_cards(body)
  local items, seen = {}, {}
  for _, m in ipairs(host.re2.find_all(
    [[(?s)<div[^>]*class="[^"]*page-listing-item[^"]*".*?post-title.*?</h[23]>]], body)) do
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

local methods = {}

-- ── search ─────────────────────────────────────────────────────────────────

methods.search = function(params)
  local q = host.text.trim(params.title or params.query or params.q or "")
  local page = math.max(1, tonumber(params.page) or 1)
  local genres = params.tags or params.genres or {}
  if type(genres) == "string" then
    local list = {}
    for name in genres:gmatch("[^,]+") do list[#list + 1] = host.text.trim(name) end
    genres = list
  end

  -- name or raw slug both accepted: resolve via the scraped nav map
  if type(genres) == "table" and #genres > 0 then
    local gslug = host.url_encode(genres_scrape().by_name[genres[1]] or genres[1])
    local gurl = BASE .. "manhua-genre/" .. gslug .. "/"
    if page > 1 then
      -- a bare /page/N/ 302s to an http:// URL that then 502s; the orderby
      -- param keeps the redirect away
      gurl = BASE .. "manhua-genre/" .. gslug .. "/page/" .. page .. "/?orderby=latest"
    end
    local body, err = http_req(gurl)
    if err ~= nil then return { ok = false, error = err } end
    local items = parse_genre_cards(body)
    return { ok = true, data = { items = items, total = #items } }
  end

  local body, err = search_json(q)
  if err ~= nil then return { ok = false, error = err } end
  local ok, decoded = pcall(json.decode, body)
  if not ok or type(decoded) ~= "table" then return { ok = true, data = { items = {}, total = 0 } } end

  -- max 6 hits; the page param is ignored by the endpoint (the host paginates).
  local items = {}
  for _, r in ipairs(decoded.data or {}) do
    local slug = r.slug
    if slug and slug ~= "" then
      items[#items + 1] = { id = slug, name = host.text.unescape(r.name or ""), cover = cover_from(r) }
    end
  end
  return { ok = true, data = { items = items, total = #items } }
end

-- ── detail ─────────────────────────────────────────────────────────────────

methods.detail = function(params)
  local slug = host.text.trim(tostring(params.id or ""))
  local empty = { id = slug, name = "", cover = "", status = "unknown", tags = {}, release_at = 0,
    description = "", authors = {}, artists = {}, url = BASE .. "manhua/" .. host.url_encode(slug) }
  if slug == "" then return { ok = true, data = empty } end

  local body, err = http_req(BASE .. "manhua/" .. host.url_encode(slug))
  if err ~= nil then return { ok = false, error = err } end

  local data = empty
  data.name = host.text.unescape(host.text.trim(find1(body, [[(?s)<div class="post-title">\s*<h1>(.*?)</h1>]]) or ""))

  local img = find1(body, [[(?s)<div class="summary_image".*?</a>]]) or ""
  data.cover = host.text.trim(find1(img, [[data-src="[\t\n\s]*(https?://[^"]+)"]])
    or find1(img, [[src="[\t\n\s]*(https?://[^"]+)"]]) or "")

  local gblock = find1(body, [[(?s)class="genres-content">(.*?)</div>]]) or ""
  local tags = {}
  for _, g in ipairs(host.re2.find_all([[<a[^>]*>([^<]+)</a>]], gblock)) do
    local name = host.text.unescape(host.text.trim(g[2] or ""))
    if name ~= "" then tags[#tags + 1] = name end
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
    if t ~= "" and t ~= data.name then paras[#paras + 1] = t end
  end
  data.description = table.concat(paras, "\n")

  local author = lookup_author(slug, data.name)
  if author ~= "" then data.authors = { author } end
  return { ok = true, data = data }
end

-- ── chapters ───────────────────────────────────────────────────────────────

methods.chapters = function(params)
  local slug = host.text.trim(tostring(params.id or ""))
  if slug == "" then return { ok = true, data = {} } end

  -- Chapter rows come from the ComicService JSON API; comicId lives only in
  -- the detail page's inline script.
  local body, err = http_req(BASE .. "manhua/" .. host.url_encode(slug))
  if err ~= nil then return { ok = false, error = err } end
  -- RE2 syntax, not Lua patterns: \s / \d, never %s / %d.
  local comic_id = find1(body, [[var comicId\s*=\s*(\d+)]]) or find1(body, [[id="comic_id"\s+value="(\d+)"]])
  if not comic_id then
    return { ok = false, error = "topmanhua: comicId missing on detail page for " .. slug }
  end

  local api, aerr = http_req(
    BASE .. "Comic/Services/ComicService.asmx/ProcessChapterList?comicId=" .. comic_id,
    { ["Referer"] = BASE .. "manhua/" .. slug })
  if aerr ~= nil then return { ok = false, error = aerr } end
  local ok, decoded = pcall(json.decode, api)
  if not ok or type(decoded) ~= "table" then
    return { ok = false, error = "topmanhua: malformed chapter list for " .. slug }
  end

  local out, seen = {}, {}
  for _, c in ipairs(decoded.chapters or {}) do
    -- the slugs are inconsistent (chapter-9-2 vs chapter-10.1): always take
    -- the API's last path segment, never rebuild it from the chapter name.
    local seg = c.url and find1(c.url, [[/([^/]+)/?$]])
    if seg and seg ~= "" and not seen[seg] then
      seen[seg] = true
      out[#out + 1] = {
        id = slug .. ":" .. seg,
        number = host.text.chapter_num(c.name or "") or 0,
        title = c.name or "",
        release_at = 0, -- API carries no dates
        pages = 0,
      }
    end
  end
  return { ok = true, data = out }
end

-- ── pages ──────────────────────────────────────────────────────────────────

methods.pages = function(params)
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

-- ── genres ─────────────────────────────────────────────────────────────────

local genre_cache = nil -- { list = {{name,slug}}, by_name = {[name]=slug} }

-- global (not local): methods.search above references it before its
-- definition point, which would otherwise compile to a nil global lookup
function genres_scrape()
  if genre_cache then return genre_cache end
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

methods.genres = function()
  local g = genres_scrape()
  local names = {}
  for _, g2 in ipairs(g.list) do names[#names + 1] = g2.name end
  table.sort(names)
  return { ok = true, data = names }
end

function handle(req)
  local fn = methods[req.method]
  if fn == nil then return { ok = false, error = "unknown method " .. tostring(req.method) } end
  return fn(req.params or {})
end
