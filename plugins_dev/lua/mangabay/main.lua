-- MangaBay (manga-bay.biz): DataLife Engine behind a PoW "guard"; search and
-- ComicList render a.cd cards, detail/reader pages carry window.__DATA__,
-- genres hide in __XFILTER__ on /comix/. Port of goIsekai
-- examples/plugins/lua/mangabay; host.re2.* replaces host.regex.* (pattern
-- first), envelope { ok, data | error }, canonical fields per the mangadex
-- plugin. Guard solving and balanced-JSON extraction live in dle.lua.
local BASE = "https://manga-bay.biz" -- manifest site_url/params, pinned like the other house ports

local dle = require("dle")

-- first capture group of the first match; nil when no match (host re2.capture).
local function find1(s, pattern)
  return host.re2.capture(pattern, tostring(s or ""))
end

-- JSON numbers decode to Lua floats, so tostring renders ids as "991476.0".
-- The site's reader paths are integral (/reader/15840/991476); the float form
-- 404s, which the host surfaces as an empty page list ("unknown chapter").
local function num_str(v)
  if type(v) ~= "number" then return tostring(v or "") end
  if v == math.floor(v) then return string.format("%d", v) end
  return tostring(v)
end

-- site_get fetches path via dle.get (guard solved over HTTP). dle.get
-- concatenates site .. path, so absolute URLs reduce to their path part.
local function site_get(path)
  if path:match("^https?://") then path = path:match("^https?://[^/]+(/.*)$") or "/" end
  return dle.get(BASE, path)
end
-- abs_url: host.url_abs anchored at BASE (D6).
local abs_url = function(path) return host.url_abs(BASE, path) end

-- path_of turns a manga id into a site path; the id is the slash-free DLE
-- slug ("11222-dungeon-meshi.html"), full permalinks accepted too.
local function path_of(id)
  id = tostring(id or "")
  if id:match("^https?://") then return id:match("^https?://[^/]+(/.*)$") or "/" end
  if id:sub(1, 1) ~= "/" then return "/" .. id end
  return id
end

-- manga_id_of strips the leading slash: host routes carry one path segment.
local function manga_id_of(href)
  return path_of(href):sub(2)
end

-- search/ComicList card (verified in the source HAR):
--   <a href="/NNN-slug.html" class="cd" title="Title">
--     <span class="cd__ph "><img data-src="/uploads/..webp" alt="Title">
local function parse_cd_cards(body)
  local items, seen = {}, {}
  local rows = host.re2.find_all(
    [[(?s)<a\s+href="([^"]+)"\s+class="cd"\s+title="([^"]*)">\s*<span class="cd__ph[^"]*">\s*<img[^>]*data-src="([^"]+)"]],
    body)
  for _, r in ipairs(rows) do
    local href, title, data_src = r[2], r[3], r[4]
    local id = dle.slug_id(href) and manga_id_of(href) or nil
    if id and not seen[id] then
      seen[id] = true
      items[#items + 1] = { id = id, name = host.text.unescape(title), cover = abs_url(data_src) }
    end
  end
  return items
end

-- the site's "Release type" wording -> host vocabulary (host.text.normalize_status
-- covers every key here, incl. on-hold/not-yet-published; empty -> "unknown").
local normalize_status = host.text.normalize_status

-- name -> DLE genre id, filled by methods.genres; search resolves UI names.
local genre_map = nil

local methods = {}

-- ── search ─────────────────────────────────────────────────────────────────

methods.search = function(params)
  local q = tostring(params.title or params.query or params.q or "")
  local page = math.max(1, tonumber(params.page) or 1)
  local tags = params.tags or params.genres or {}
  if type(tags) == "string" then -- UI joins checked genres with commas
    local list = {}
    for name in tags:gmatch("[^,]+") do list[#list + 1] = name:match("^%s*(.-)%s*$") end
    tags = list
  end

  local body
  if type(tags) == "table" and #tags > 0 then
    -- Genre browse: /ComicList/g=<id>/page/<N>/. First genre wins; the id is
    -- the DLE genre number (genres map resolves UI names, digits pass through).
    local g = tostring(tags[1] or "")
    local gid = (genre_map and genre_map[g]) or g:match("(%d+)")
    if not gid then return { ok = true, data = { items = {}, total = 0 } } end
    local url = "/ComicList/g=" .. tostring(gid)
    if page > 1 then url = url .. "/page/" .. tostring(page) .. "/" end
    body = site_get(url .. "/")
  elseif q ~= "" then
    local url = "/search/" .. host.url_encode(q)
    if page > 1 then url = url .. "/page/" .. tostring(page) end
    body = site_get(url)
  else
    return { ok = true, data = { items = {}, total = 0 } }
  end

  local items = parse_cd_cards(body or "")
  return { ok = true, data = { items = items, total = #items } }
end

-- ── detail ─────────────────────────────────────────────────────────────────
-- <header class="page__header"><h1>Title</h1>, <dl> rows author/artist/status,
-- genres row; __DATA__ carries machine dates for chapters only, never series.

methods.detail = function(params)
  local id = tostring(params.id or "")
  local body = site_get(path_of(id))
  if body == "" then return { ok = false, error = "mangabay: detail fetch failed" } end

  local title = find1(body, [[<header class="page__header">\s*<h1>([^<]+)</h1>]])
    or find1(body, [[<title>([^<]+)</title>]]) or ""
  title = host.text.trim((host.text.unescape(title):gsub("%s*Manga%-?Bay.*$", "")))

  local alt_titles = find1(body, [[<p class="page__alt">(.*?)</p>]]) or ""

  -- the poster <img> is sometimes wrapped in an <a> ("Open full cover")
  local cover = find1(body, [[<div class="page__poster[^"]*"[^>]*>\s*<img[^>]*src="([^"]+)"]])
    or find1(body, [[<div class="page__poster[^"]*"[^>]*>\s*<a[^>]*>\s*<img[^>]*src="([^"]+)"]])
  cover = abs_url(cover)

  local desc = find1(body, [[<div class="page__desc-text full-text clearfix">(.*?)</div>]]) or ""
  desc = host.text.unescape(host.text.strip_html(desc))

  local author = find1(body, [[<dt>Author</dt><dd>([^<]+)</dd>]]) or ""
  local artist = find1(body, [[<dt>Artist</dt><dd>([^<]+)</dd>]]) or ""
  if author == "" then author = artist end

  local status = normalize_status(find1(body, [[<dt>Status</dt><dd>([^<]+)</dd>]]))

  local genres = {}
  local genre_row = find1(body, [[<dt>Genres</dt><dd>(.*?)</dd>]]) or ""
  for _, r in ipairs(host.re2.find_all([[<a[^>]*>([^<]+)</a>]], genre_row)) do
    local label = host.text.trim(r[2] or "")
    if label ~= "" then genres[#genres + 1] = label end
  end
  if #genres == 0 then -- JSON-LD ComicSeries fallback
    local ld = dle.comic_series(body)
    if ld and type(ld.genre) == "string" then
      for graw in ld.genre:gmatch("([^,]+)") do
        local g = host.text.trim(graw)
        if g ~= "" then genres[#genres + 1] = g end
      end
    end
  end

  -- the alt-title line is the only place the other names live; keep it as a
  -- description tail (the ABI has one description field)
  alt_titles = host.text.trim(host.text.unescape(alt_titles))
  if alt_titles ~= "" then desc = desc .. "\n\nAlternative titles: " .. alt_titles end

  return { ok = true, data = {
    id = id,
    name = title,
    cover = cover,
    status = status,
    tags = genres,
    release_at = 0, -- detail markup has no machine-readable series date
    description = desc,
    authors = author ~= "" and { author } or {},
    url = BASE .. path_of(id),
  } }
end

-- ── chapters ───────────────────────────────────────────────────────────────
-- __DATA__.chapters[] {id,posi,pages,title,date,broken}; posi is the number,
-- the array is newest-first, so sort ascending for the reader.

methods.chapters = function(params)
  local id = tostring(params.id or "")
  local body = site_get(path_of(id))
  local data = dle.json_data(body, "window.__DATA__ = ")
  local rows = data and type(data.chapters) == "table" and data.chapters or nil
  local out = {}
  if rows then
    for _, c in ipairs(rows) do
      if not c.broken then
        out[#out + 1] = {
          -- the id IS the reader path; pages receives it back verbatim
          id = "/reader/" .. num_str(data.news_id) .. "/" .. num_str(c.id),
          number = tonumber(c.posi) or host.text.chapter_num(c.title or "") or 0,
          title = c.title or ("Issue #" .. tostring(c.id)),
          release_at = dle.dle_epoch(c.date),
          pages = tonumber(c.pages) or 0,
        }
      end
    end
    table.sort(out, function(a, b) return a.number < b.number end)
  end
  return { ok = true, data = out }
end

-- ── pages ──────────────────────────────────────────────────────────────────
-- Reader __DATA__.images[] capped by .pages (the reader truncates: images=9,
-- pages=8). Pages are plain URL strings; komacdn hotlink-guards on Referer,
-- which per-page image headers are a host-side concern (see report).

methods.pages = function(params)
  local reader = tostring(params.id or "")
  if reader == "" then return { ok = true, data = {} } end
  -- chapters rows cached before the num_str fix carry the float ids
  -- (/reader/15840.0/991476.0); normalize so those sessions still resolve.
  reader = reader:gsub("%.0$", ""):gsub("%.0/", "/")
  local body = site_get(path_of(reader))
  local data = dle.json_data(body, "window.__DATA__ = ")
  local images = data and type(data.images) == "table" and data.images or {}
  local count = tonumber(data and data.pages) or #images
  if count > #images then count = #images end
  local out = {}
  for i = 1, count do
    if type(images[i]) == "string" and images[i] ~= "" then out[#out + 1] = images[i] end
  end
  return { ok = true, data = out }
end

-- ── genres ─────────────────────────────────────────────────────────────────
-- /comix/ __XFILTER__.filter_items.g.values [{id,value}]; the id feeds
-- /ComicList/g=<id>/, the name is what the UI shows and sends back.

methods.genres = function()
  local body = site_get("/comix/")
  local f = dle.json_data(body, "__XFILTER__ = ")
  local values = f and f.filter_items and f.filter_items.g and f.filter_items.g.values or nil
  local names, map = {}, {}
  if type(values) == "table" then
    for _, v in ipairs(values) do
      if type(v.value) == "string" and v.value ~= "" then
        names[#names + 1] = v.value
        map[v.value] = tostring(v.id)
      end
    end
  end
  genre_map = map
  table.sort(names)
  return { ok = true, data = names }
end

function handle(req)
  local fn = methods[req.method]
  if fn == nil then return { ok = false, error = "unknown method " .. tostring(req.method) } end
  return fn(req.params or {})
end
