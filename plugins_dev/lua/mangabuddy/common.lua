-- MangaBuddy (mangabuddy1.co.uk): Laravel reader, no JS challenge — browser-UA
-- GETs suffice. Text search is a JSON endpoint, genre browse is HTML cards,
-- chapters a JSON endpoint keyed by bare slug. Port of goIsekai
-- examples/plugins/lua/mangabuddy; host.re2.* replaces host.regex.* (pattern
-- first), envelope { ok, data | error }, canonical fields per the mangadex plugin.
local BASE = "https://mangabuddy1.co.uk" -- manifest site_url; .com is a status/mirror tracker, the reader lives here

local helpers = require("helpers")
local genres = require("genres")

-- first capture group of the first match; nil when no match (host re2.capture).

-- full manga id is "{slug}.{zid}"; several endpoints want the bare slug

-- Info rows carry their value as a link target, not a text node:
--   ...<h?>Label</h?> ... <a href="/series?status=Ongoing"> / <a href="/author/NAME">
-- re2 pins the label, then a plain substring scan reads the next href
-- (host.re2.find_index has no start-offset argument).

-- RFC3339 timestamps -> unix seconds via host.text.date_epoch (D7); 0 when
-- absent/unparseable, the house zero-time contract.


-- ── search ─────────────────────────────────────────────────────────────────


-- ── detail ─────────────────────────────────────────────────────────────────


-- ── chapters ───────────────────────────────────────────────────────────────
-- /get-chapter-list?slug=<bare> -> {success, data:[{chapter_slug,chapter_num,
-- chapter_name,updated_at}]}, newest-first; sorted ascending for the reader.


-- ── pages ──────────────────────────────────────────────────────────────────
-- Chapter id "slug.ZID:chapter_slug" -> /series/slug.ZID/chapter_slug; page
-- images are data-src webp's. The CDN host is matched loosely on purpose (it
-- has moved before); cover/ad images lack the slug/ch/page shape. Pages are
-- bare URL strings — the CDN 403s without a Referer, a host-side concern.


-- ── genres ─────────────────────────────────────────────────────────────────


local function find1(s, pattern)
  return host.re2.capture(pattern, tostring(s or ""))
end

local function bare_slug(manga_id)
  return manga_id:match("^(.-)%.")
end

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

local function epoch(s)
  return host.text.date_epoch(tostring(s or "")) or 0
end

return { BASE=BASE, helpers=helpers, genres=genres, find1=find1, bare_slug=bare_slug, label_value=label_value, epoch=epoch }
