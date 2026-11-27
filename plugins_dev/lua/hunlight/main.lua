-- hunlightcomics.com: WordPress Madara theme, HTML scraping.
-- Permalinks are rewritten to /m/<slug>/ (not the stock /manga/), chapters come
-- from the Madara ajax endpoint, images from img.wp-manga-chapter-img. The site
-- exposes no genre archives (/manga-genre/<g>/ = 404) and no genre markup on
-- the title page, so search is the only discovery path and `genres` is absent.
local BASE = "https://hunlightcomics.com"
local fetch = host.http.fetch_body

-- Plain-Lua block splitter (comik lesson: anchor with find(..., true) and slice,
-- don't lean on lazy regex over thousands of bytes).
local function blocks(body, marker, cap)
  local out, pos = {}, 1
  while true do
    local s = body:find(marker, pos, true)
    if not s then
      break
    end
    local e = body:find(marker, s + 1, true) or (#body + 1)
    out[#out + 1] = body:sub(s, math.min(e - 1, s + cap))
    pos = e
  end
  return out
end

local function text(s)
  return host.text.unescape(host.text.trim(s or ""))
end

-- ── search ─────────────────────────────────────────────────────────────────
-- Result cards are `row c-tabs-item__content` blocks (cover anchor first, then
-- post-title h3). Empty query = the /manga/ archive, which pages by
-- /manga/page/N/ and prints no pager, only the "Older Posts" link.
local CARD = 'class="row c-tabs-item__content"'
local ARCHIVE_CARD = 'class="page-item-detail'

-- WP thumbnail URLs carry a -WxH suffix (incubus1-193x278.jpg); the un-suffixed
-- file is the original, portrait and high-res -- the -1200x630 og:image variant
-- is a landscape crop that the 2:3 UI boxes would zoom into.
local function fullsize(u)
  return u and u:gsub("-%d+x%d+(%.[%a]+)$", "%1") or u
end

local function harvest(body, marker, items, seen)
  for _, card in ipairs(blocks(body, marker, 3000)) do
    local slug = card:match('href="' .. BASE .. '/m/([%w%-]+)/"')
    local name = card:match('<h3[^>]*>%s*<a[^>]*>([^<]+)</a>')
    if slug and name and not seen[slug] then
      seen[slug] = true
      items[#items + 1] = {
        id = slug,
        name = text(name),
        cover = fullsize(text(card:match('<img[^>]-src="([^"]+)"'))),
      }
    end
  end
end

local function search(params)
  local q = params.title or params.query or params.q or ""
  local page = math.max(1, tonumber(params.page) or 1)
  local url = BASE .. "/?s=" .. host.url_encode(q) .. "&post_type=wp-manga"
  if q == "" then
    -- No site search for "everything": the manga archive is the browse list.
    url = page > 1 and (BASE .. "/manga/page/" .. page .. "/") or (BASE .. "/manga/")
  end
  local body, err = fetch(url)
  if not body then
    return { ok = false, error = err or "hunlight: search failed" }
  end

  local items, seen = {}, {}
  harvest(body, CARD, items, seen)
  if #items == 0 then
    harvest(body, ARCHIVE_CARD, items, seen) -- the /manga/ archive renders its own shape
  end
  -- Search ignores ?page=N (verified: identical 10 cards for page 1..3), so the
  -- result set is one page; the archive knows its next page by URL.
  local pages = 1
  if q == "" and body:find("/manga/page/" .. (page + 1) .. "/", 1, true) then
    pages = page + 1
  end
  return { ok = true, data = { items = items, total = #items, pages = pages } }
end

-- ── detail ─────────────────────────────────────────────────────────────────
local function detail(params)
  local slug = params.id or ""
  if slug == "" then
    return { ok = false, error = "hunlight: missing id" }
  end
  local body, err = fetch(BASE .. "/m/" .. slug .. "/")
  if not body then
    return { ok = false, error = err or "hunlight: detail failed" }
  end

  local name = (body:match("<title[^>]*>([^<]*)</title>") or slug):gsub("%s*&#8211;%s*Hunlight%s*$", "")
  local tags = {}
  local authors, artists = {}, {}
  for a in body:gmatch('manga%-author/[^"]-/" rel="tag">([^<]+)<') do
    authors[#authors + 1] = text(a)
  end
  for a in body:gmatch('manga%-artist/[^"]-/" rel="tag">([^<]+)<') do
    artists[#artists + 1] = text(a)
  end
  local syn = body:match('<div class="description%-summary">.-<p>(.-)</p>')
  local status = body:match("Status%s*</h5>%s*</div>%s*<div class=\"summary%-content\">%s*([^<%s][^<]-)%s*<")
    or ""

  return {
    ok = true,
    data = {
      id = slug,
      name = text(name),
      -- summary_image thumb first (portrait original after suffix-strip);
      -- og:image is the 1200x630 landscape crop WP mints for social embeds
      cover = fullsize(text(body:match('summary_image.-src="([^"]+)"')))
        or text(body:match('property="og:image" content="([^"]+)"')),
      status = host.text.normalize_status(status),
      tags = tags, -- none on this site
      release_at = 0,
      description = syn and text(host.text.strip_html(syn)) or "",
      authors = authors,
      artists = artists,
      url = BASE .. "/m/" .. slug .. "/",
    },
  }
end

-- ── chapters ───────────────────────────────────────────────────────────────
-- POST /m/<slug>/ajax/chapters/ (Madara; the X-Requested-With header is the
-- gate). Rows carry a human release date, parsed to unix seconds host-side.
local function chapters(params)
  local slug = params.id or ""
  if slug == "" then
    return { ok = false, error = "hunlight: missing id" }
  end
  local body, err = fetch(BASE .. "/m/" .. slug .. "/ajax/chapters/", {
    method = "POST",
    body = "",
    headers = { ["X-Requested-With"] = "XMLHttpRequest" },
  })
  if not body then
    return { ok = false, error = err or "hunlight: chapters failed" }
  end

  local out, seen = {}, {}
  for _, li in ipairs(blocks(body, 'class="wp-manga-chapter', 1200)) do
    local cid = li:match('href="' .. BASE .. '/m/[%w%-]+/([%w%-]+)/"')
    local label = li:match("<a[^>]*>%s*([^<]-)%s*</a>")
    if cid and label and not seen[cid] then
      seen[cid] = true
      out[#out + 1] = {
        id = slug .. ":" .. cid, -- pages needs both halves
        number = host.text.chapter_num(label) or 0,
        title = text(label),
        release_at = host.text.date_epoch(li:match("<i>([^<]+)</i>") or "") or 0,
        pages = 0,
      }
    end
  end
  if #out == 0 then
    return { ok = false, error = "hunlight: no chapters for " .. slug }
  end
  table.sort(out, function(a, b)
    return (a.number or 0) < (b.number or 0)
  end)
  return { ok = true, data = out }
end

-- ── pages ──────────────────────────────────────────────────────────────────
local function pages(params)
  local slug, cid = tostring(params.id or ""):match("^([%w%-]+):([%w%-]+)$")
  if not slug then
    return { ok = false, error = "hunlight: chapter id must be slug:chapter, got " .. tostring(params.id) }
  end
  local body, err = fetch(BASE .. "/m/" .. slug .. "/" .. cid .. "/")
  if not body then
    return { ok = false, error = err or "hunlight: pages failed" }
  end

  -- Madara lazy themes park the real URL in data-src, eager ones in src.
  local srcs = host.html.attr(body, "img.wp-manga-chapter-img", "data-src")
  if #srcs == 0 then
    srcs = host.html.attr(body, "img.wp-manga-chapter-img", "src")
  end
  local out = {}
  for _, s in ipairs(srcs) do
    local url = host.text.trim(s)
    if url ~= "" then
      out[#out + 1] = url
    end
  end
  if #out == 0 then
    return { ok = false, error = "hunlight: no pages for " .. tostring(params.id) }
  end
  return { ok = true, data = out }
end

local methods = { search = search, detail = detail, chapters = chapters, pages = pages }

function handle(req)
  local fn = methods[req.method]
  if fn == nil then
    return { ok = false, error = "unknown method " .. tostring(req.method) }
  end
  return fn(req.params or {})
end
