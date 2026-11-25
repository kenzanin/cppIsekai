-- dynasty-scans.com (Dynasty Reader): Rails site; everything but search has a
-- JSON twin at the same path (.json suffix). Port of goIsekai
-- examples/plugins/lua/dynastyscan to the cppIsekai host ABI (host.http.fetch
-- + status check; host.re2.* pattern-first). Pages are plain URL strings.
local BASE = "https://dynasty-scans.com" -- single-domain; main.json param kept verbatim, unused

-- host.text.trim: utf8-aware, case-preserving (plugin-host-promotion).
local trim = host.text.trim




-- ── search ─────────────────────────────────────────────────────────────────
-- No /search.json: search (and genre browse) scrape HTML. Tag archives need
-- view=groupings or only /chapters links appear and the series match finds
-- nothing (source lesson, kept).

-- ── detail ─────────────────────────────────────────────────────────────────
-- /series/<slug>.json also carries the chapter list ("taggings"), so detail
-- and chapters share one request shape.

-- ── chapters ───────────────────────────────────────────────────────────────

-- ── pages ──────────────────────────────────────────────────────────────────

-- ── genres ─────────────────────────────────────────────────────────────────
-- Live from /tags so the list cannot drift from what archive URLs accept;
-- tag links appear nowhere but series pages, the index is the only source.

local function get_json(url)
  return (host.http.get_json(url, { headers = { accept = "application/json" } }))
end

local function absolute(path)
  if path == nil or path == "" then return "" end
  local p = tostring(path)
  if p:match("^https?://") then return p end
  return BASE .. p
end

return { BASE=BASE, trim=trim, get_json=get_json, absolute=absolute }
