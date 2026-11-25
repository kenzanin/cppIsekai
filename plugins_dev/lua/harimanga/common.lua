-- HariManga (harimanga.cc): WordPress Madara theme, HTML scraping; Cloudflare-fronted.
-- Port of goIsekai examples/plugins/lua/harimanga; adapted to cppIsekai host tools
-- (host.http.fetch replaces host.http.get_body/post_body, host.re2.* replaces
-- host.regex.* with arg order pattern,subject). Envelope { ok, data | error };
-- canonical manga fields per the mangadex plugin.
local BASE = "https://www.harimanga.cc" -- single origin; manifest site_url kept verbatim in main.json

-- http request -> (body, err). 200-or-nothing, the anisascans http_req shape.
local http_req = host.http.fetch_body -- (body, err); nil unless 200 (plugin-host-promotion).

-- first capture group of the first match; nil when no match (host re2.capture).


-- name -> slug map filled by methods.genres; search resolves UI genre names.
local genre_map = nil

-- ── search ─────────────────────────────────────────────────────────────────


-- ── detail ─────────────────────────────────────────────────────────────────


-- ── chapters ───────────────────────────────────────────────────────────────


-- ── pages ──────────────────────────────────────────────────────────────────


-- ── genres ─────────────────────────────────────────────────────────────────


local function find1(s, pattern)
  return host.re2.capture(pattern, tostring(s or ""))
end

return { BASE=BASE, http_req=http_req, genre_map=genre_map, find1=find1 }
