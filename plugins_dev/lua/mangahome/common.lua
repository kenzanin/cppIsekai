-- MangaHome (www.mangahome.com): MangaHere-mirror SSR; plain <img> reader
-- pages, "Apr 05,2024" chapter dates. Port of goIsekai
-- examples/plugins/lua/mangahome; native string patterns kept throughout (the
-- source used no re2 either), envelope { ok, data | error }, canonical fields
-- per the mangadex plugin. The site serves a JS shell to non-browser agents,
-- so every content fetch carries the Chrome UA below.
local BASE = "https://www.mangahome.com" -- manifest site_url, kept literal

local genres = require("genres")

local BROWSER_UA =
  "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36"

-- request -> body or nil (200-only); the UA is load-bearing, not cosmetic

-- "Apr 05,2024" etc -> unix seconds via host.text.date_epoch (D7); 0 when
-- unparseable, the house zero-time contract.

-- ── search ─────────────────────────────────────────────────────────────────

-- ── detail ─────────────────────────────────────────────────────────────────

-- ── chapters ───────────────────────────────────────────────────────────────
-- rows: <a href="/manga/<slug>/cNNN" ...>...<span class="time">Apr 05,2024</span>

-- ── pages ──────────────────────────────────────────────────────────────────
-- chapter id "<slug>:<num>"; the reader page embeds /store/manga/ jpgs directly

-- ── genres ─────────────────────────────────────────────────────────────────

local function http_body(url)
  return (host.http.fetch_body(url, { headers = { ["User-Agent"] = BROWSER_UA } }))
end

local function date_epoch(s)
  return host.text.date_epoch(tostring(s or "")) or 0
end

return { BASE = BASE, genres = genres, BROWSER_UA = BROWSER_UA, http_body = http_body, date_epoch = date_epoch }
