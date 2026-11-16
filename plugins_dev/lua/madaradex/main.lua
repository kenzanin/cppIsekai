-- MadaraDex (madaradex.org): WordPress Madara theme, manga under /title/;
-- the chapter list is inline on the detail page (MangaPage mode, no ajax).
-- Port of goIsekai examples/plugins/lua/madaradex; host.re2.* replaces
-- host.regex.* (pattern-first args), fetch+per-plugin cookie jar replaces
-- get_body/post_body (the site refreshes an mdx_auth cookie via admin-ajax
-- and expects it on every later request -> cookie=true everywhere).
-- Envelope { ok, data | error }; canonical manga fields per the mangadex plugin.
local BASE = "https://madaradex.org" -- manifest site_url
local AJAX = BASE .. "/wp-admin/admin-ajax.php"
local MANGA_PATH = "/title/"

-- One handshake per load: MadaraDex intercepts requests while mdx_auth is
-- missing, the POST reply sets it in the per-plugin jar.
local auth_done = false
local function warm_auth()
  if auth_done then return end
  auth_done = true
  host.http.fetch(AJAX, {
    method = "POST",
    body = "action=mdx_auth_refresh",
    headers = {
      ["X-Mdx-Auth-Refresh"] = "1",
      ["X-Requested-With"] = "xmlhttprequest",
      ["Content-Type"] = "application/x-www-form-urlencoded",
    },
    cookie = true,
  })
end

-- http request -> (body, err); cookie jar on every call (mdx_auth).
local function http_req(url, opts)
  warm_auth()
  local o = {}
  if opts then for k, v in pairs(opts) do o[k] = v end end
  o.cookie = true
  local res, err = host.http.fetch(url, o)
  if err ~= nil then return nil, err end
  if res.status ~= 200 then return nil, "madaradex: upstream status " .. tostring(res.status) end
  return res.body, nil
end

-- first capture group of the first match; nil when the pattern does not match.
local function find1(s, pattern)
  local m = host.re2.find(pattern, tostring(s or ""))
  if #m >= 2 then return m[2] end
  if #m == 1 then return m[1] end
  return nil
end

-- href can be absolute or site-relative; the reader needs absolute.
local function abs_url(raw)
  raw = tostring(raw or "")
  if raw == "" then return "" end
  if raw:match("^https?://") then return raw end
  if raw:match("^//") then return "https:" .. raw end
  return BASE .. "/" .. raw:gsub("^/+", "")
end

local function manga_url(slug)
  return BASE .. MANGA_PATH .. host.url_encode(slug) .. "/"
end

-- Lazy attributes win over src, in the order Madara emits them.
local function img_src(fragment)
  if not fragment or fragment == "" then return "" end
  local v = find1(fragment, [[data-src="\s*(https?://[^"]+)]])
    or find1(fragment, [[data-src="\s*(/[^"]+)]])
    or find1(fragment, [[data-lazy-src="\s*(https?://[^"]+)]])
    or find1(fragment, [[data-cfsrc="\s*(https?://[^"]+)]])
    or find1(fragment, [[data-manga-src="\s*(https?://[^"]+)]])
    or find1(fragment, [[src="\s*(https?://[^"]+)]])
    or find1(fragment, [[src="\s*(/[^"]+)]])
  if v then return abs_url(host.text.trim(v)) end
  return ""
end

local methods = {}

-- name -> slug map filled by methods.genres; search resolves UI genre names.
local genre_map = nil

-- ── search ─────────────────────────────────────────────────────────────────

methods.search = function(params)
  local q = host.text.trim(tostring(params.title or params.query or params.q or ""))
  local page = math.max(1, tonumber(params.page) or 1)
  local tags = params.tags or params.genres or {}
  if type(tags) == "string" then
    local list = {}
    for name in tags:gmatch("[^,]+") do list[#list + 1] = name:match("^%s*(.-)%s*$") end
    tags = list
  end

  local url
  if type(tags) == "table" and #tags > 0 then
    -- theme genre archive, server-rendered (NOT /manga-genre/ like siblings)
    local slug = (genre_map and genre_map[tags[1]]) or tags[1]
    url = BASE .. "/genre/" .. host.url_encode(slug) .. "/"
    if page > 1 then url = url .. "page/" .. tostring(page) .. "/" end
  else
    -- plain WordPress GET search; this theme's admin-ajax loader answers
    -- "no-posts" for query strings
    url = BASE .. "/?s=" .. host.url_encode(q) .. "&post_type=wp-manga"
  end

  local body, err = http_req(url)
  if err ~= nil then return { ok = false, error = err } end

  local items, seen = {}, {}
  -- two card layouts (search grid c-tabs-item__content, genre page
  -- page-listing-item) share the post-title block; one tolerant wrapper
  for _, m in ipairs(host.re2.find_all(
    [[(?s)<div[^>]*class="[^"]*(?:c-tabs-item__content|page-listing-item)[^"]*".*?post-title.*?</h3>]], body)) do
    local card = m[1]
    local href = find1(card, [[href="([^"]*/title/[^"/]+/)]])
    local title = find1(card, [[(?s)<h3[^>]*>.*?<a[^>]*>(.*?)</a>]])
    local slug = href and href:match("/title/([^/]+)/?$")
    if slug and slug ~= "" and not seen[slug] then
      seen[slug] = true
      local cover = find1(card, [[data-src="\s*(https?://[^"]+)]])
        or find1(card, [[\ssrc="(https?://[^"]+)]])
      items[#items + 1] = {
        id = slug,
        name = host.text.unescape(host.text.strip_html(title or "")),
        cover = cover and host.text.trim(cover) or "",
        url = abs_url(href),
      }
    end
  end
  return { ok = true, data = { items = items, total = #items } }
end

-- ── detail ─────────────────────────────────────────────────────────────────

methods.detail = function(params)
  local slug = host.text.trim(tostring(params.id or ""))
  if slug == "" then return { ok = true, data = {} } end
  local body, err = http_req(manga_url(slug))
  if err ~= nil then return { ok = false, error = err } end

  local tblock = find1(body, [[(?s)class="post-title[^"]*">(.*?)</div>]]) or ""
  local name = host.text.unescape(host.text.strip_html(find1(tblock, [[(?s)<h[1-6][^>]*>(.*?)</h[1-6]>]]) or ""))
  local author = host.text.unescape(host.text.strip_html(find1(body, [[(?s)class="author-content">(.*?)</div>]]) or ""))
  local cover = img_src(find1(body, [[(?s)class="summary_image".*?</a>]]) or "")

  local desc = find1(body, [[(?s)class="summary__content[^"]*">(.*?)<span\s+class="[^"]*content-readmore"]]) or ""
  if desc == "" then
    desc = find1(body, [[(?s)class="summary__content[^"]*">(.*?)</div>]]) or ""
  end
  desc = host.text.unescape(host.text.trim(host.text.strip_html(desc))):gsub("%s+", " ")

  local tags = {}
  for _, g in ipairs(host.re2.find_all([[<a[^>]*>([^<]+)</a>]], find1(body, [[(?s)class="genres-content">(.*?)</div>]]))) do
    local label = host.text.unescape(host.text.trim(g[2] or ""))
    if label ~= "" then tags[#tags + 1] = label end
  end

  local status = ""
  local sblock = find1(body, [[(?s)Status\s*</h5>.*?class="summary-content">\s*([^<]+)]])
  if sblock then status = host.text.normalize_status(host.text.trim(sblock)) end

  return { ok = true, data = {
    id = slug,
    name = name,
    cover = cover,
    status = status,
    tags = tags,
    release_at = 0, -- Madara dates are human strings; no machine date here
    description = desc,
    authors = author ~= "" and { author } or {},
    url = manga_url(slug),
  } }
end

-- ── chapters (MangaPage mode: listed on the detail page) ───────────────────

methods.chapters = function(params)
  local slug = host.text.trim(tostring(params.id or ""))
  if slug == "" then return { ok = true, data = {} } end
  local body, err = http_req(manga_url(slug))
  if err ~= nil then return { ok = false, error = err } end

  local chapters, seen = {}, {}
  for _, m in ipairs(host.re2.find_all([[(?s)<li[^>]*class="wp-manga-chapter[^"]*"[^>]*>(.*?)</li>]], body)) do
    local li = m[1]
    local href = find1(li, [[href="([^"]+)"]])
    local cid = href and find1(href, [[/([^/]+)/?$]])
    local label = host.text.trim(host.text.strip_html(find1(li, [[(?s)<a[^>]*>(.*?)</a>]]) or ""))
    if href and cid and cid ~= "" and not seen[cid] then
      seen[cid] = true
      chapters[#chapters + 1] = {
        id = slug .. ":" .. cid,
        number = host.text.chapter_num(label) or 0,
        title = label,
        release_at = 0, -- Madara dates are human strings
        pages = 0,
      }
    end
  end
  table.sort(chapters, function(a, b) return (a.number or 0) < (b.number or 0) end)
  return { ok = true, data = chapters }
end

-- ── pages ──────────────────────────────────────────────────────────────────

methods.pages = function(params)
  local id = tostring(params.id or "")
  local slug, cslug = id:match("^(.-):(.+)$")
  if not slug or slug == "" or not cslug or cslug == "" then return { ok = true, data = {} } end

  local url = manga_url(slug) .. host.url_encode(cslug) .. "/"
  local body, err = http_req(url)
  if err ~= nil then return { ok = false, error = err } end

  -- the paged reader carries one image; the list layout has them all
  if body:find('id="single-pager"', 1, true) then
    local listed, lerr = http_req(url .. "?style=list")
    if lerr == nil and listed and listed ~= "" then body = listed end
  end

  -- tag-at-a-time scan (host.re2.find_index has no start-offset argument)
  local pages, scanned, pos = {}, 0, 1
  while true do
    local s = body:find("<img", pos, true)
    if not s then break end
    local e = body:find(">", s, true)
    if not e then break end
    local tag = body:sub(s, e)
    scanned = scanned + 1
    if tag:find("wp-manga-chapter-img", 1, true) then
      local src = img_src(tag)
      if src ~= "" then pages[#pages + 1] = src end -- cdn.madaradex.org 403s without Referer; host sends site origin
    end
    pos = e + 1
  end

  -- parsed page but no reader markup: broken upstream chapter, name it
  if #pages == 0 and scanned > 0 then
    return { ok = false, error = "no_pages: " .. cslug }
  end
  return { ok = true, data = pages }
end

-- ── genres ─────────────────────────────────────────────────────────────────
-- Scraped from the /title/ navigation so the list cannot drift; archive path
-- is /genre/<slug>/ on this theme (not /manga-genre/).

methods.genres = function()
  local body, err = http_req(BASE .. MANGA_PATH)
  if err ~= nil then return { ok = false, error = err } end
  local seen, names, map = {}, {}, {}
  -- the label is not always bare text (sometimes span-wrapped): capture to </a>
  for _, m in ipairs(host.re2.find_all([[href="[^"]*/genre/([a-z0-9_-]+)/?"[^>]*>(.*?)</a>]], body)) do
    local slug, label = m[2], host.text.unescape(host.text.trim(host.text.strip_html(m[3] or "")))
    if slug ~= "" and label ~= "" and not seen[slug] then
      seen[slug] = true
      names[#names + 1] = label
      map[label] = slug
    end
  end
  table.sort(names)
  genre_map = map
  return { ok = true, data = names }
end

function handle(req)
  local fn = methods[req.method]
  if fn == nil then return { ok = false, error = "unknown method " .. tostring(req.method) } end
  return fn(req.params or {})
end
