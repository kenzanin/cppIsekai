-- SanaScans plugin (sanascans.com): SSR-first TanStack Start reader, no API.
-- Port of goIsekai examples/plugins/lua/sanascans; host.text.unescape replaces
-- host.text.html_decode, the unused b64e export is dropped. Chapter dates are
-- real: the series page embeds YYYY-MM-DD per row, converted to unix seconds.
-- Envelope { ok, data | error }.
local BASE = "https://sanascans.com"

-- ── helpers ────────────────────────────────────────────────────────────────

local http_req = host.http.fetch_body -- (body, err); nil unless 200 (plugin-host-promotion).

-- Extract a slug from a series path like /series/<slug> or <slug>.
local function slug_of(path)
  if not path then return nil end
  local s = string.match(path, "/series/([^/]+)") or path
  return string.match(s, "([^/?#]+)")
end

-- First ISO date in a chunk of text.
local function first_iso(chunk)
  return (string.match(chunk or "", "%d%d%d%d%-%d%d%-%d%d")) or nil
end

-- UTC midnight epoch for "YYYY-MM-DD" (Howard Hinnant's days-from-civil; the
-- sandbox has no os lib). 0 when nothing parses.
local function epoch_date(s)
  local y, m, d = tostring(s or ""):match("(%d%d%d%d)-(%d%d)-(%d%d)")
  if not y then return 0 end
  y, m, d = tonumber(y), tonumber(m), tonumber(d)
  local yy = y - (m <= 2 and 1 or 0)
  local era = math.floor(yy / 400)
  local yoe = yy - era * 400
  local doy = math.floor((153 * (m + (m > 2 and -3 or 9)) + 2) / 5) + d - 1
  local doe = yoe * 365 + math.floor(yoe / 4) - math.floor(yoe / 100) + doy
  return (era * 146097 + doe - 719468) * 86400
end

local methods = {}

-- ── search ─────────────────────────────────────────────────────────────────

methods.search = function(params)
  local q = params.title or params.query or params.q or ""
  if q == "" then return { ok = true, data = { items = {}, total = 0 } } end
  local body, err = http_req(BASE .. "/comics?search=" .. host.url_encode(q))
  if err ~= nil then return { ok = false, error = err } end

  -- Cards: <a href="/series/<slug>" ...><img ... alt="Cover of <title>" src=
  -- "<cover>"> (attribute order is alt BEFORE src; the site prefixes many card
  -- alts with "Cover of " — strip it so titles stay clean).
  local items, seen = {}, {}
  for _, m in ipairs(host.re2.find_all(
    [[(?s)<a href="/series/([^"]+)"[^>]*>.*?<img[^>]*alt="([^"]*)"[^>]*src="([^"]+)"]], body)) do
    -- The slug is URL-encoded AND HTML-escaped inside the href (the site's
    -- React-style link, e.g. "violetta&#x27;s-...%3A..."): unescape, then
    -- url_decode; detail/chapter re-encode once.
    local slug = host.url_decode(host.text.unescape(m[2]))
    local title = string.gsub(m[3], "^Cover [Oo]f%s+", "")
    if not seen[slug] then
      seen[slug] = true
      items[#items + 1] = { id = slug, name = host.text.unescape(host.text.trim(title)), cover = m[4] }
    end
  end
  return { ok = true, data = { items = items, total = #items } }
end

-- ── detail ─────────────────────────────────────────────────────────────────

methods.detail = function(params)
  local slug = params.id or ""
  local empty = { id = slug, name = "", cover = "", status = "unknown", tags = {}, release_at = 0,
    description = "", authors = {}, artists = {}, url = BASE .. "/series/" .. host.url_encode(slug) }
  if slug == "" then return { ok = true, data = empty } end

  local body, err = http_req(BASE .. "/series/" .. host.url_encode(slug))
  if err ~= nil then return { ok = false, error = err } end

  -- Real cover: the itemProp="image" img src (full art). og:image on this
  -- site is a generated og-card with title overlay — cropped, so fallback only.
  local cover = host.text.unescape((string.match(body, 'itemProp="image"[^>]-src="([^"]+)"')
    or string.match(body, 'property="og:image" content="([^"]+)"')
    or string.match(body, '"image":"([^"]+)"')) or "")
  local title = host.text.unescape((string.match(body, 'property="og:title" content="([^"]+)"')
    or string.match(body, '"name":"([^"]*)"')) or slug)
  -- strip a trailing "Manga - Sana Scans" suffix if present
  title = string.match(title, "(.-)%s*[Mm]anga%s*-%s*[Ss]ana") or title
  local description = host.text.unescape((string.match(body, 'property="og:description" content="([^"]*)"')
    or string.match(body, '"description":"([^"]*)"')) or "")
  local status = host.text.unescape(string.match(body, '"status":"([^"]*)"') or "")

  local tags = {}
  -- (source captured the KEY "genre"/"category" instead of the value; fixed)
  for _, v in string.gmatch(body, '"(genre|category)"%s*:%s*"([^"]*)"') do
    if v ~= "" then tags[#tags + 1] = v end
  end

  local data = empty
  data.name = title
  data.cover = cover
  data.status = host.text.normalize_status(status)
  data.tags = tags
  data.description = description
  return { ok = true, data = data }
end

-- ── chapters ───────────────────────────────────────────────────────────────

methods.chapters = function(params)
  local slug = params.id or ""
  if slug == "" then return { ok = true, data = {} } end
  local body, err = http_req(BASE .. "/series/" .. host.url_encode(slug))
  if err ~= nil then return { ok = false, error = err } end

  -- Chapter rows: <a href="/series/<slug>/chapter-N">...Ch. N...</a> plus a
  -- <time datetime>/<YYYY-MM-DD> sibling within the same row; anchor on the
  -- link and read the next date in a bounded tail.
  local out = {}
  for _, m in ipairs(host.re2.find_all(
    [[(?s)<a href="(/series/[^"/]+/chapter-[\d.]+)"[^>]*>(.*?)</a>(.{0,400})]], body)) do
    local path, cell, tail = m[2], m[3] or "", m[4] or ""
    local chid = string.match(path, "chapter%-[%d.]+") or string.match(path, "/([^/]+)$") or path
    -- chapter number: from the visible "Ch. N" text, else the URL tail
    local num = string.match(cell, "Ch%.%s*([%d.]+)") or string.match(chid, "([%d.]+)$")
    -- date: <time datetime> in the link or row tail, else first ISO date
    local raw_date = string.match(cell, '<time[^>]*datetime="([^"]+)"')
      or string.match(tail, '<time[^>]*datetime="([^"]+)"')
      or first_iso(tail) or first_iso(cell)
    local release_at = 0
    if raw_date then release_at = epoch_date(host.text.trim(raw_date)) end
    out[#out + 1] = {
      id = slug .. ":" .. chid,
      number = tonumber(num) or 0,
      title = host.text.trim(string.match(cell, ">%s*([^<]*)%s*<!--") or string.match(cell, ">%s*([^<]*)") or ""),
      release_at = release_at,
      pages = 0,
    }
  end
  -- sanascans lists newest first; keep document order.
  return { ok = true, data = out }
end

-- ── pages ──────────────────────────────────────────────────────────────────

methods.pages = function(params)
  local composite = params.id or ""
  local slug, chid = string.match(composite, "^(.-):(.+)$")
  if not slug or not chid or slug == "" then
    return { ok = false, error = "sanascans: chapter id must be slug:chapterId, got " .. composite }
  end
  local body, err = http_req(BASE .. "/series/" .. host.url_encode(slug) .. "/" .. chid)
  if err ~= nil then return { ok = false, error = err } end

  -- Reader pages are SSR <img src> in a scroll container; the site emits a
  -- double slash after the host, so scan with plain substring finds (a re2
  -- anchor stops at the host) and rebuild the URL without the duplicate.
  local out = {}
  local anchor = 'src="https://media.sanascans.com/' -- site renamed storage.->media. (2026-11, checked live)
  local pos = string.find(body, anchor, 1, true)
  while pos do
    local ustart = pos + #anchor
    local close = string.find(body, '"', ustart, true)
    if not close then break end
    local path = string.sub(body, ustart, close - 1)
    if string.find(path, "upload/series/", 1, true) then
      local u = string.gsub("https://media.sanascans.com/" .. path, "(https://[^/]+)/+", "%1/")
      out[#out + 1] = u
    end
    pos = string.find(body, anchor, close, true)
  end
  if #out == 0 then return { ok = false, error = "sanascans: no pages for " .. composite } end
  return { ok = true, data = out }
end

function handle(req)
  local fn = methods[req.method]
  if fn == nil then return { ok = false, error = "unknown method " .. tostring(req.method) } end
  return fn(req.params or {})
end
