-- en-thunderscans.com: WordPress "mangareader" theme (bsx cards, ts_reader).
-- Port of goIsekai examples/plugins/lua/en-thunderscan to the cppIsekai host
-- ABI (host.http.fetch + status check; host.re2.* pattern-first). Pages are
-- plain URL strings; genres are static site data (genres.lua).
local BASE = "https://en-thunderscans.com" -- single-domain; main.json param kept verbatim, unused

-- host.text.trim: utf8-aware, case-preserving (plugin-host-promotion).
local trim = host.text.trim

-- re2.find returns {} (truthy) on no-match; take the capture so `or` chains
-- and nil-check behave like goIsekai's find.
local function cap(pat, s, n)
  return host.re2.find(pat, tostring(s or ""))[n or 2]
end

local function http_body(url)
  local b = host.http.fetch_body(url)
  if b == nil or b == "" then return nil end
  return b
end

local methods = {}

-- ── search ─────────────────────────────────────────────────────────────────
-- bsx cards: <div class="bsx"><a href=".../comics/<slug>/">...<img>...<div class="tt">Title</div>
methods.search = function(params)
  local query = tostring(params.title or params.query or params.q or "")
  local page = tonumber(params.page) or 1
  local genres = type(params.tags) == "table" and params.tags
    or type(params.genres) == "table" and params.genres or {}

  local url
  if #genres > 0 then
    local g = genres[1]
    if type(g) == "table" then g = g.slug or g.name end
    -- the host echoes the genres() name back; browse routes key on the slug
    local gslug = tostring(g):lower():gsub("%s+", "-")
    url = BASE .. "/genres/" .. host.url_encode(gslug) .. "/"
    if page > 1 then
      url = url .. "page/" .. tostring(page) .. "/"
    end
  elseif page > 1 then
    url = BASE .. "/page/" .. tostring(page) .. "/?s=" .. host.url_encode(query)
  else
    url = BASE .. "/?s=" .. host.url_encode(query)
  end

  local body = http_body(url)
  if not body then
    return { ok = false, error = "en-thunderscans: search fetch failed" }
  end

  local items, seen = {}, {}
  for _, card_m in ipairs(host.re2.find_all([[(?s)<div class="bsx".*?<div class="tt">.*?</div>]], body)) do
    local card = card_m[1]
    local href = cap([[href="([^"]+/comics/[^"/]+/)"]], card) or cap([[href="([^"]+)"]], card)
    local title = cap([[<div class="tt">\s*([^<]+)\s*</div>]], card) or cap([[title="([^"]+)"]], card)
    if href then
      local slug = cap([[/comics/([^/]+)/]], href) or cap([[/([^/]+)/?$]], href)
      if slug and not seen[slug] then
        seen[slug] = true
        local cover = cap([[data-src="[\t\n\s]*(https?://[^"]+)"]], card)
          or cap([[\ssrc="(https?://[^"]+)"]], card, 2)
        items[#items + 1] = {
          id = slug,
          name = host.text.unescape(trim(title)),
          cover = cover and trim(cover) or "",
        }
      end
    end
  end
  return { ok = true, data = { items = items, total = #items } }
end

-- ── detail/chapters shared fetch ───────────────────────────────────────────
local function fetch_detail_body(slug)
  local body = http_body(BASE .. "/comics/" .. slug .. "/")
  if body and cap([[entry-title]], body, 1) then
    return body
  end
  -- fallback: flat URL without the /comics/ prefix
  return http_body(BASE .. "/" .. slug .. "/") or body
end

-- ── detail ─────────────────────────────────────────────────────────────────
methods.detail = function(params)
  local slug = tostring(params.id or "")
  local body = fetch_detail_body(slug)
  if not body then
    return { ok = true, data = { id = slug } }
  end

  local name = host.text.unescape(trim(cap([[(?s)<h1 class="entry-title"[^>]*>(.*?)</h1>]], body) or ""))

  local thumb = cap([[(?s)<div class="thumb"[^>]*>(.*?)</div>]], body) or ""
  local cover = cap([[data-src="(https?://[^"]+)"]], thumb) or cap([[src="(https?://[^"]+)"]], thumb) or ""

  local author = host.text.unescape(trim(
    cap([[(?s)<div class="imptdt">\s*<h1>\s*Author\s*</h1>\s*<i>([^<]+)</i>]], body) or ""))

  local genres = {}
  local gblock = cap([[(?s)<span class="mgen">(.*?)</span>]], body) or ""
  for _, m in ipairs(host.re2.find_all([[<a[^>]*>([^<]+)</a>]], gblock)) do
    local fname = host.text.unescape(trim(m[2]))
    if fname ~= "" then genres[#genres + 1] = fname end
  end

  local status = trim(cap([[(?s)<div class="imptdt">\s*<h1>\s*Status\s*</h1>\s*<i>([^<]+)</i>]], body) or "")

  local dblock = cap([[(?s)entry-content entry-content-single"[^>]*>(.*?)</div>]], body) or ""
  local description = host.re2.replace([[\s+]], " ", host.text.strip_html(dblock))

  return { ok = true, data = {
    id = slug,
    name = name,
    cover = trim(cover),
    status = host.text.normalize_status(status),
    tags = genres,
    -- theme renders no machine date on the series page
    release_at = 0,
    description = description,
    authors = author ~= "" and { author } or {},
    artists = {},
    url = BASE .. "/comics/" .. slug .. "/",
  } }
end

-- ── chapters ───────────────────────────────────────────────────────────────
-- Free rows: <li data-num="40"><a href=...>-chapter-40/...chapternum...chapterdate...</a></li>.
-- Coin-locked rows (lockedChapterModal) carry no <a href> — skipped.
methods.chapters = function(params)
  local slug = tostring(params.id or "")
  local body = fetch_detail_body(slug)
  if not body then
    return { ok = true, data = {} }
  end

  local chapters, seen = {}, {}
  for _, row in ipairs(host.re2.find_all([[(?s)<li data-num="([^"]+)"[^>]*>(.*?)</li>]], body)) do
    local num, li = row[2], row[3]
    local href = cap([[href="([^"]+)"]], li)
    if href and not cap([[lockedChapterModal]], li, 1) then
      local label = trim(cap([[(?s)chapternum[^>]*>\s*([^<]+)]], li) or "")
      label = trim(host.re2.replace([[\s+]], " ", label))
      if label == "" then label = "Chapter " .. num end
      local date = trim(cap([[(?s)chapterdate[^>]*>\s*([^<]+)]], li) or "")
      local cid = cap([[/([^/]+)/?$]], href)
      if cid and not seen[cid] then
        seen[cid] = true
        chapters[#chapters + 1] = {
          id = cid,
          number = host.text.chapter_num(label) or 0,
          title = host.text.unescape(label),
          -- "January 6, 2021"-style; host.text.date_to_iso parses it
          release_at = host.text.date_to_iso(date) or 0,
          pages = 0,
        }
      end
    end
  end
  return { ok = true, data = chapters }
end

-- ── pages ──────────────────────────────────────────────────────────────────
-- Reader payload: ts_reader.run({"sources":[{"source":"..","images":[...]}]}).
-- id is the path segment under the domain ("series-slug/chapter-slug" style
-- collapsed by the caller into the trailing path).
methods.pages = function(params)
  local path = tostring(params.id or "")
  local body = http_body(BASE .. "/" .. path .. "/")
  if not body then
    return { ok = false, error = "en-thunderscans: reader fetch failed" }
  end

  local run = cap([[(?s)ts_reader\.run\((\{.*?\})\)]], body) or ""
  local imgblock = cap([=[(?s)"images"\s*:\s*\[(.*?)\]]=], run) or ""

  local out = {}
    for _, m in ipairs(host.re2.find_all([[(https?:[^\"]+?\.(webp|jpg|jpeg|png|avif))]], imgblock)) do
      local src = trim(host.re2.replace([[\\]], "", m[2]))
      if src ~= "" then out[#out + 1] = src end
  end
  if #out == 0 then
    return { ok = false, error = "en-thunderscans: no pages for " .. path }
  end
  return { ok = true, data = out }
end

-- ── genres ─────────────────────────────────────────────────────────────────
methods.genres = function()
  return { ok = true, data = require("genres").names }
end

function handle(req)
  local fn = methods[req.method]
  if fn == nil then return { ok = false, error = "unknown method " .. tostring(req.method) } end
  return fn(req.params or {})
end
