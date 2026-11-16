-- MangaBuddy (mangabuddy1.co.uk): Laravel reader, no JS challenge — browser-UA
-- GETs suffice. Text search is a JSON endpoint, genre browse is HTML cards,
-- chapters a JSON endpoint keyed by bare slug. Port of goIsekai
-- examples/plugins/lua/mangabuddy; host.re2.* replaces host.regex.* (pattern
-- first), envelope { ok, data | error }, canonical fields per the mangadex plugin.
local BASE = "https://mangabuddy1.co.uk" -- manifest site_url; .com is a status/mirror tracker, the reader lives here

local helpers = require("helpers")
local genres = require("genres")

-- first capture group of the first match; nil when the pattern does not match.
local function find1(s, pattern)
  local m = host.re2.find(pattern, tostring(s or ""))
  if #m >= 2 then return m[2] end
  if #m == 1 then return m[1] end
  return nil
end

-- full manga id is "{slug}.{zid}"; several endpoints want the bare slug
local function bare_slug(manga_id)
  return manga_id:match("^(.-)%.")
end

-- Info rows carry their value as a link target, not a text node:
--   ...<h?>Label</h?> ... <a href="/series?status=Ongoing"> / <a href="/author/NAME">
-- re2 pins the label, then a plain substring scan reads the next href
-- (host.re2.find_index has no start-offset argument).
local function label_value(html, label, href_prefix)
  local m = host.re2.find(label .. [[\s*</h1>]], html)
  if not m or #m == 0 then return "" end
  local fin = html:find(m[1], 1, true)
  if not fin then return "" end
  local seg = html:sub(fin + #m[1], fin + #m[1] + 400)
  local at = seg:find('href="' .. href_prefix, 1, true)
  if not at then return "" end
  return seg:match('([^"]+)', at + 6 + #href_prefix) or ""
end

-- ISO timestamps -> epoch (Hinnant civil-days, as mangadex); relative phrases
-- and missing dates collapse to 0 rather than dropping the chapter.
local function epoch(s)
  if not s then return 0 end
  local y, mo, d, h, mi, sec = tostring(s):match("(%d+)-(%d+)-(%d+)T(%d+):(%d+):(%d+)")
  if not y then return 0 end
  y, mo, d, h, mi, sec = tonumber(y), tonumber(mo), tonumber(d), tonumber(h), tonumber(mi), tonumber(sec)
  local yy = y - (mo <= 2 and 1 or 0)
  local era = math.floor(yy / 400)
  local yoe = yy - era * 400
  local doy = math.floor((153 * (mo + (mo > 2 and -3 or 9)) + 2) / 5) + d - 1
  local doe = yoe * 365 + math.floor(yoe / 4) - math.floor(yoe / 100) + doy
  local days = era * 146097 + doe - 719468
  return days * 86400 + h * 3600 + mi * 60 + sec
end

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

  local items = {}
  if type(tags) == "table" and #tags > 0 then
    -- Genre browse renders HTML comic cards; first genre wins.
    local slug = tostring(tags[1] or "")
    slug = genres.map[slug] or slug
    local url = BASE .. "/series?genres=" .. host.url_encode(slug)
    if page > 1 then url = url .. "&page=" .. tostring(page) end
    local body = helpers.http_get(url)
    if not body then return { ok = false, error = "mangabuddy: genre browse failed" } end
    -- <a href="/series/{id}"> ... <img alt="Cover of {title}" data-src="{cover}">
    local rows = host.re2.find_all(
      [[(?s)<a[^>]*href="/series/([^/?"]+)"[^>]*>.*?<img[^>]*alt="Cover of ([^"]+)"[^>]*data-src="([^"]+)"]],
      body)
    for _, r in ipairs(rows) do
      if r[2] and r[2] ~= "" then
        items[#items + 1] = { id = r[2], name = host.text.unescape(r[3] or ""), cover = host.text.trim(r[4] or "") }
      end
    end
  else
    -- Text search: /api/search?search=Q -> {comics:[{slug_hash,title,image,status}]}
    local raw = helpers.http_get(BASE .. "/api/search?search=" .. host.url_encode(q), { json = true })
    if not raw then return { ok = false, error = "mangabuddy: search failed" } end
    local ok, data = pcall(json.decode, raw)
    if ok and type(data) == "table" and type(data.comics) == "table" then
      for _, c in ipairs(data.comics) do
        if type(c.slug_hash) == "string" and c.slug_hash ~= "" then
          items[#items + 1] = { id = c.slug_hash, name = c.title or "", cover = c.image or "" }
        end
      end
    end
  end
  return { ok = true, data = { items = items, total = #items } }
end

-- ── detail ─────────────────────────────────────────────────────────────────

methods.detail = function(params)
  local id = tostring(params.id or "")
  local html = helpers.http_get(BASE .. "/series/" .. id)
  if not html then return { ok = false, error = "mangabuddy: detail fetch failed" } end

  local title = find1(html, [[<meta property="og:title" content="([^"]*)"]]) or ""
  title = title:gsub("^Read%s+", "")
  title = title:gsub("%s*|%s*MangaBuddy%s*$", "")
  title = title:gsub("%s+Online$", "")
  title = host.text.unescape(title)

  local cover = find1(html, [[<meta property="og:image" content="([^"]*)"]]) or ""

  local desc = find1(html, [[<meta name="description" content="([^"]*)"]]) or ""
  desc = desc:gsub("^Read%s+[^%.]+%.%s*", "")
  desc = desc:gsub("%s*Read the latest chapters online for free at MangaBuddy%.?%s*$", "")
  desc = host.text.unescape(desc)

  local status = label_value(html, "Status", "/series?status=")
  local author = label_value(html, "Author", "/author/")
  if author == "Unknown" then author = "" end

  -- genre chips: href="/genre/{slug}"; the label is slug-cased, so retitle
  -- each word (re2.replace has no function replacement)
  local tags, seen_g = {}, {}
  for _, r in ipairs(host.re2.find_all([[href="/genre/([a-z0-9-]+)"]], html)) do
    local g = r[2]
    if g and g ~= "" and not seen_g[g] then
      seen_g[g] = true
      local label = g:gsub("%-", " ")
      label = label:gsub("(%a)([%w']*)", function(a, b) return a:upper() .. b end)
      tags[#tags + 1] = label
    end
  end

  return { ok = true, data = {
    id = id,
    name = title,
    cover = cover,
    status = helpers.normalizeStatus(status),
    tags = tags,
    release_at = 0, -- the page carries no machine-readable series date
    description = desc,
    authors = author ~= "" and { author } or {},
    url = BASE .. "/series/" .. id,
  } }
end

-- ── chapters ───────────────────────────────────────────────────────────────
-- /get-chapter-list?slug=<bare> -> {success, data:[{chapter_slug,chapter_num,
-- chapter_name,updated_at}]}, newest-first; sorted ascending for the reader.

methods.chapters = function(params)
  local manga_id = tostring(params.id or "")
  local slug = bare_slug(manga_id)
  if not slug or slug == "" then return { ok = true, data = {} } end
  local raw = helpers.http_get(BASE .. "/get-chapter-list?slug=" .. host.url_encode(slug), { json = true })
  if not raw then return { ok = false, error = "mangabuddy: chapter list fetch failed" } end
  local ok, body = pcall(json.decode, raw)
  if not ok or type(body) ~= "table" or not body.success or type(body.data) ~= "table" then
    return { ok = false, error = "mangabuddy: bad chapter payload for " .. slug }
  end

  local out = {}
  for _, ch in ipairs(body.data) do
    local cslug = ch.chapter_slug or ""
    -- chapter_num is missing on some rows; the slug still ends in the number
    local num = tonumber(ch.chapter_num) or host.text.chapter_num(cslug) or 0
    if num > 0 or cslug ~= "" then -- rows with neither carry no usable id
      out[#out + 1] = {
        id = manga_id .. ":" .. (cslug ~= "" and cslug or ("chapter-" .. tostring(num))),
        number = num,
        title = ch.chapter_name or ("Chapter " .. tostring(num)),
        release_at = epoch(ch.updated_at),
        pages = 0, -- the list endpoint carries no page count
      }
    end
  end
  table.sort(out, function(a, b) return a.number < b.number end)
  return { ok = true, data = out }
end

-- ── pages ──────────────────────────────────────────────────────────────────
-- Chapter id "slug.ZID:chapter_slug" -> /series/slug.ZID/chapter_slug; page
-- images are data-src webp's. The CDN host is matched loosely on purpose (it
-- has moved before); cover/ad images lack the slug/ch/page shape. Pages are
-- bare URL strings — the CDN 403s without a Referer, a host-side concern.

methods.pages = function(params)
  local id = tostring(params.id or "")
  local manga_id, chslug = id:match("^(.-):(.+)$")
  if not manga_id or manga_id == "" or not chslug or chslug == "" then
    return { ok = true, data = {} }
  end
  local html = helpers.http_get(BASE .. "/series/" .. manga_id .. "/" .. chslug)
  if not html then return { ok = false, error = "mangabuddy: page fetch failed" } end

  local out, seen = {}, {}
  for _, r in ipairs(host.re2.find_all([[data-src="(https://[^"/]+/[^"]*?/[\d.]+/\d+\.webp)"]], html)) do
    local u = r[2]
    if u and u ~= "" and not seen[u] then
      seen[u] = true
      out[#out + 1] = u
    end
  end
  return { ok = true, data = out }
end

-- ── genres ─────────────────────────────────────────────────────────────────

methods.genres = function()
  return { ok = true, data = genres.names }
end

function handle(req)
  local fn = methods[req.method]
  if fn == nil then return { ok = false, error = "unknown method " .. tostring(req.method) } end
  return fn(req.params or {})
end
