-- ValirScans plugin (valirscans.org): custom Next.js reader, not WordPress.
-- Port of goIsekai examples/plugins/lua/valirscans; host.re2 argument order is
-- (pattern, subject). host.text.json_blob has no cppIsekai equivalent — ld+json
-- blocks are bracket-extracted by regex instead. The manifest "media" param is
-- unused by the source (page URLs are absolute) and is dropped here too.
-- Envelope { ok, data | error }.
local BASE = "https://valirscans.org"

-- ── helpers ────────────────────────────────────────────────────────────────

local http_req = host.http.fetch_body -- (body, err); nil unless 200 (plugin-host-promotion).

-- first capture group of the first match; nil when no match (host re2.capture).


-- First decoded ld+json block whose "@type" is "Book", or nil. The page
-- carries several ld+json blocks; the lazy match stays inside one tag because
-- JSON-LD never contains "</script>".

-- Series status lives in the flight payload right after the series'
-- coverImage entry: read the first "status":"X" following the cover path.
-- (goIsekai bounded this to a 600-char window via find_index; first-match-after
-- is equivalent in practice and needs no second primitive.)


-- ── search ─────────────────────────────────────────────────────────────────


-- ── detail ─────────────────────────────────────────────────────────────────


-- ── chapters ───────────────────────────────────────────────────────────────


-- ── pages ──────────────────────────────────────────────────────────────────


local function find1(s, pattern)
  return host.re2.capture(pattern, tostring(s or ""))
end

local function is_abs_url(s)
  return #host.re2.find("^https?://", tostring(s or "")) > 0
end

local function jsonld_book(html)
  for _, m in ipairs(host.re2.find_all(
    [[(?s)<script type="application/ld\+json">(.*?)</script>]], html)) do
    local ok, obj = pcall(json.decode, m[2])
    if ok and type(obj) == "table" and obj["@type"] == "Book" then return obj end
  end
  return nil
end

local function flight_status(html, cover_path)
  if not cover_path or cover_path == "" then return "" end
  local pat = '(?s)\\"coverImage\\":\\"' .. host.re2.quote(cover_path) .. '.*?\\"status\\":\\"([A-Z_]+)\\"'
  local m = host.re2.find(pat, html)
  if #m >= 2 then return m[2] end
  return ""
end

return { BASE=BASE, http_req=http_req, find1=find1, is_abs_url=is_abs_url, jsonld_book=jsonld_book, flight_status=flight_status }
