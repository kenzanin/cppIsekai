-- MangaRead plugin (www.mangaread.org): WordPress Madara theme, HTML scraping.
-- Port of goIsekai examples/plugins/lua/mangaread; adapted to cppIsekai host
-- tools (host.http.fetch, host.re2). Search parser unified with the anisascans
-- card capture (same theme, verified selector). Envelope { ok, data | error }.
local BASE = "https://www.mangaread.org"

local GENRE_PATH = "/genres/" -- this theme's archive root (anisascans: /manga-genre/)

-- ── helpers ────────────────────────────────────────────────────────────────

-- http request -> (body, err). cppIsekai host.http.fetch replaces goIsekai get_body.
local http_req = host.http.fetch_body -- (body, err); nil unless 200 (plugin-host-promotion).

-- first capture group of the first match; nil when no match (host re2.capture).
local function find1(s, pattern)
  return host.re2.capture(pattern, tostring(s or ""))
end

-- ── search ─────────────────────────────────────────────────────────────────

return { BASE = BASE, GENRE_PATH = GENRE_PATH, http_req = http_req, find1 = find1 }
