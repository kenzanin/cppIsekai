-- SanaScans plugin (sanascans.com): SSR-first TanStack Start reader, no API.
-- Port of goIsekai examples/plugins/lua/sanascans; host.text.unescape replaces
-- host.text.html_decode, the unused b64e export is dropped. Chapter dates are
-- real: the series page embeds YYYY-MM-DD per row, converted to unix seconds.
-- Envelope { ok, data | error }.
local BASE = "https://sanascans.com"

-- ── helpers ────────────────────────────────────────────────────────────────

local http_req = host.http.fetch_body -- (body, err); nil unless 200 (plugin-host-promotion).

-- Extract a slug from a series path like /series/<slug> or <slug>.

-- First ISO date in a chunk of text.

-- UTC midnight epoch for "YYYY-MM-DD" (Howard Hinnant's days-from-civil; the
-- sandbox has no os lib). 0 when nothing parses.


-- ── search ─────────────────────────────────────────────────────────────────

-- The site has NO server-side search: /comics?search= ignores the param (the
-- SSR ships the same 188-anchor catalog for every value) and /search 404s.
-- The only search surface is the navbar live-dropdown JSON API (har:
-- api.sanascans.com/api/query?searchTerm=&perPage=4), which also paginates.
local API = "https://api.sanascans.com"


-- ── detail ─────────────────────────────────────────────────────────────────


-- ── chapters ───────────────────────────────────────────────────────────────


-- ── pages ──────────────────────────────────────────────────────────────────


local function slug_of(path)
  if not path then return nil end
  local s = string.match(path, "/series/([^/]+)") or path
  return string.match(s, "([^/?#]+)")
end

local function first_iso(chunk)
  return (string.match(chunk or "", "%d%d%d%d%-%d%d%-%d%d")) or nil
end

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

return { BASE=BASE, http_req=http_req, API=API, slug_of=slug_of, first_iso=first_iso, epoch_date=epoch_date }
