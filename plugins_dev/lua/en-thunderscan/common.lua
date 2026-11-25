-- en-thunderscans.com: WordPress "mangareader" theme (bsx cards, ts_reader).
-- Port of goIsekai examples/plugins/lua/en-thunderscan to the cppIsekai host
-- ABI (host.http.fetch + status check; host.re2.* pattern-first). Pages are
-- plain URL strings; genres are static site data (genres.lua).
local BASE = "https://en-thunderscans.com" -- single-domain; main.json param kept verbatim, unused

-- host.text.trim: utf8-aware, case-preserving (plugin-host-promotion).
local trim = host.text.trim

-- re2.find returns {} (truthy) on no-match; take the capture so `or` chains
-- and nil-check behave like goIsekai's find.

-- ── search ─────────────────────────────────────────────────────────────────
-- bsx cards: <div class="bsx"><a href=".../comics/<slug>/">...<img>...<div class="tt">Title</div>

-- ── detail/chapters shared fetch ───────────────────────────────────────────

-- ── detail ─────────────────────────────────────────────────────────────────

-- ── chapters ───────────────────────────────────────────────────────────────
-- Free rows: <li data-num="40"><a href=...>-chapter-40/...chapternum...chapterdate...</a></li>.
-- Coin-locked rows (lockedChapterModal) carry no <a href> — skipped.

-- ── pages ──────────────────────────────────────────────────────────────────
-- Reader payload: ts_reader.run({"sources":[{"source":"..","images":[...]}]}).
-- id is the path segment under the domain ("series-slug/chapter-slug" style
-- collapsed by the caller into the trailing path).

-- ── genres ─────────────────────────────────────────────────────────────────

local function cap(pat, s, n)
  return host.re2.find(pat, tostring(s or ""))[n or 2]
end

local function http_body(url)
  local b = host.http.fetch_body(url)
  if b == nil or b == "" then
    return nil
  end
  return b
end

local function fetch_detail_body(slug)
  local body = http_body(BASE .. "/comics/" .. slug .. "/")
  if body and cap([[entry-title]], body, 1) then
    return body
  end
  -- fallback: flat URL without the /comics/ prefix
  return http_body(BASE .. "/" .. slug .. "/") or body
end

return { BASE = BASE, trim = trim, cap = cap, http_body = http_body, fetch_detail_body = fetch_detail_body }
