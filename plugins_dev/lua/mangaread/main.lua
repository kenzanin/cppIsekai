-- MangaRead plugin (www.mangaread.org): WordPress Madara theme, HTML scraping.
-- Port of goIsekai examples/plugins/lua/mangaread; adapted to cppIsekai host
-- tools (host.http.fetch, host.re2). Search parser unified with the anisascans
-- card capture (same theme, verified selector). Envelope { ok, data | error }.
local BASE = "https://www.mangaread.org"

local GENRE_PATH = "/genres/" -- this theme's archive root (anisascans: /manga-genre/)

-- ── helpers ────────────────────────────────────────────────────────────────

-- http request -> (body, err). cppIsekai host.http.fetch replaces goIsekai get_body.
local function http_req(url, opts)
  local res, err = host.http.fetch(url, opts or {})
  if err ~= nil then return nil, err end
  if res.status ~= 200 then return nil, "mangaread: upstream status " .. tostring(res.status) end
  return res.body, nil
end

-- first capture group of the first match; nil when the pattern does not match.
local function find1(s, pattern)
  local m = host.re2.find(pattern, tostring(s or ""))
  if #m >= 2 then return m[2] end
  if #m == 1 then return m[1] end
  return nil
end

local methods = {}

-- ── search ─────────────────────────────────────────────────────────────────

methods.search = function(params)
  local q = params.title or params.query or params.q or ""
  local page = math.max(1, tonumber(params.page) or 1)
  local genres = params.tags or params.genres or {}
  if type(genres) == "string" then
    local list = {}
    for name in genres:gmatch("[^,]+") do list[#list + 1] = host.text.trim(name) end
    genres = list
  end

  local enc_q = host.url_encode(q)
  local genre_slug
  if type(genres) == "table" and #genres > 0 then genre_slug = require("genres").map[genres[1]] or genres[1] end

  -- Madara page 2+ is /page/N/ (?page=N is not supported) and admin-ajax.php
  -- answers 0 bytes, so both branches use the server-rendered GET.
  local function mkurl(p)
    if genre_slug then
      local u = BASE .. GENRE_PATH .. host.url_encode(genre_slug) .. "/"
      if p > 1 then return u .. "page/" .. tostring(p) .. "/" end
      return u
    end
    if p > 1 then return BASE .. "/page/" .. tostring(p) .. "/?s=" .. enc_q .. "&post_type=wp-manga" end
    return BASE .. "/?s=" .. enc_q .. "&post_type=wp-manga"
  end

  -- Site-wide count from the SERP heading ("170 results for \"dungeon\"");
  -- the 12 cards on this page are not the total.
  local function parse_total(b)
    local raw = (find1(b, [[([0-9,]+)\s+results?\s+for]]) or ""):gsub(",", "")
    return tonumber(raw)
  end

  local body, err = http_req(mkurl(page))
  if err ~= nil then
    -- The site overcounts its own pager ("170 results" but /page/15/ is 404):
    -- serve an empty tail page carrying page 1's count so prev still works.
    if page > 1 and err:find("404", 1, true) then
      return { ok = true, data = { items = {}, total = parse_total(http_req(mkurl(1)) or "") or page * 12 } }
    end
    return { ok = false, error = err }
  end

  local items, seen = {}, {}
  -- one card per c-tabs-item__content block: cover <img> first, then post-title
  -- link. Capture up to the post-title close so the cover stays in the same card.
  for _, m in ipairs(host.re2.find_all([[(?s)<div[^>]*class="[^"]*c-tabs-item__content.*?post-title.*?</h3>]], body)) do
    local card = m[1]
    local href = find1(card, [[href="([^"]*/manga/[^"/]+/)"]])
    local title = find1(card, [[<h3[^>]*><a[^>]*>([^<]+)</a>]])
    if href then
      local slug = find1(href, [[/manga/([^/]+)/]]) or find1(href, [[/manga/([^/]+)$]])
      if slug and not seen[slug] then
        seen[slug] = true
        local cover = find1(card, [[data-src="[\t\n\s]*(https?://[^"]+)"]])
          or find1(card, [[\ssrc="(https?://[^"]+)"]])
        items[#items + 1] = { id = slug, name = host.text.unescape(title or ""), cover = host.text.trim(cover or "") }
      end
    end
  end
  return { ok = true, data = { items = items, total = parse_total(body) or #items } }
end

-- ── detail ─────────────────────────────────────────────────────────────────

methods.detail = function(params)
  local slug = params.id or ""
  local empty = { id = slug, name = "", cover = "", status = "unknown", tags = {}, release_at = 0,
    description = "", authors = {}, artists = {}, url = BASE .. "/manga/" .. slug .. "/" }
  if slug == "" then return { ok = true, data = empty } end

  local body, err = http_req(BASE .. "/manga/" .. slug .. "/")
  if err ~= nil then return { ok = false, error = err } end

  local cover_block = find1(body, [[(?s)class="summary_image".*?</a>]]) or ""
  local cover = host.text.trim(find1(cover_block, [[data-src="([^"]+)"]]) or find1(cover_block, [[src="([^"]+)"]]) or "")

  local tblock = find1(body, [[(?s)class="post-title[^"]*">(.*?)</div>]]) or ""
  local title = host.text.unescape(host.text.trim(find1(tblock, [[(?s)<h[1-6][^>]*>(.*?)</h[1-6]>]]) or ""))

  local ablock = find1(body, [[(?s)class="author-content">(.*?)</div>]]) or ""
  local author = host.text.strip_html(ablock):gsub(",", ", ")

  local gblock = find1(body, [[(?s)class="genres-content">(.*?)</div>]]) or ""
  local tags = {}
  for _, g in ipairs(host.re2.find_all([[<a[^>]*>([^<]+)</a>]], gblock)) do
    local name = host.text.unescape(host.text.trim(g[2] or ""))
    if name ~= "" then tags[#tags + 1] = name end
  end

  local status = "unknown"
  local sblock = find1(body, [[(?s)class="post-status">(.*?)$]]) or ""
  if sblock ~= "" then
    local after = find1(sblock, [[(?s)Status\s*</h5>.*?class="summary-content">\s*([^<]+)]])
    if after then status = host.text.normalize_status(after) end
  end

  local dblock = find1(body, [[(?s)class="summary__content[^"]*">(.*?)<span\s+class="[^"]*content-readmore"]]) or ""
  if dblock == "" then dblock = find1(body, [[(?s)class="summary__content[^"]*">(.*?)</div>]]) or "" end
  local description = host.text.strip_html(dblock):gsub("%s+", " ")

  local data = empty
  data.name = title
  data.cover = cover
  data.status = status
  data.tags = tags
  data.description = description
  if author ~= "" then data.authors = { author } end
  return { ok = true, data = data }
end

-- ── chapters ───────────────────────────────────────────────────────────────

methods.chapters = function(params)
  local slug = params.id or ""
  if slug == "" then return { ok = true, data = {} } end
  -- Madara chapters via POST /manga/<slug>/ajax/chapters/.
  local body, err = http_req(BASE .. "/manga/" .. slug .. "/ajax/chapters/", {
    method = "POST",
    body = "",
    headers = { ["X-Requested-With"] = "xmlhttprequest", ["Content-Type"] = "application/x-www-form-urlencoded" },
  })
  if err ~= nil then return { ok = false, error = err } end

  local out, seen = {}, {}
  for _, m in ipairs(host.re2.find_all([[(?s)<li[^>]*class="wp-manga-chapter[^"]*"[^>]*>(.*?)</li>]], body)) do
    local li = m[1]
    local href = find1(li, [[href="([^"]+)"]])
    local label = host.text.trim(find1(li, [[>([^<]*[Cc]hapter[^<]*)<]]) or "")
    if href and label ~= "" then
      local cid = find1(href, [[/manga/[^/]+/([^/]+)/?$]]) or find1(href, [[/([^/]+)/?$]])
      if cid and not seen[cid] then
        seen[cid] = true
        out[#out + 1] = {
          id = slug .. ":" .. cid, -- composite slug:chapterId, pages needs both
          number = host.text.chapter_num(label) or 0,
          title = label,
          release_at = 0, -- Madara dates are human strings; anisascans precedent keeps 0
          pages = 0,
        }
      end
    end
  end

  -- fallback: chapter links in the page body
  if #out == 0 then
    for _, m in ipairs(host.re2.find_all([[<a[^>]*href="([^"]*chapter-[^"]+)"[^>]*>\s*([^<]+)\s*</a>]], body)) do
      local href, label = m[2], m[3]
      local cid = find1(href, [[/([^/]+)/?$]])
      if cid and not seen[cid] then
        seen[cid] = true
        out[#out + 1] = { id = slug .. ":" .. cid, number = host.text.chapter_num(label) or 0,
          title = host.text.trim(label), release_at = 0, pages = 0 }
      end
    end
  end

  table.sort(out, function(a, b) return (a.number or 0) < (b.number or 0) end)
  return { ok = true, data = out }
end

-- ── pages ──────────────────────────────────────────────────────────────────

methods.pages = function(params)
  local composite = params.id or ""
  local slug, cid = composite:match("^([^:]+):(.+)$")
  if not slug or not cid then
    return { ok = false, error = "mangaread: chapter id must be slug:chapterId, got " .. tostring(params.id) }
  end
  local body, err = http_req(BASE .. "/manga/" .. slug .. "/" .. cid .. "/")
  if err ~= nil then return { ok = false, error = err } end

  -- Madara page images carry class wp-manga-chapter-img; lazy themes put the
  -- URL in data-src, eager ones in src.
  local out = {}
  local srcs = host.html.attr(body, "img.wp-manga-chapter-img", "data-src")
  if #srcs == 0 then srcs = host.html.attr(body, "img.wp-manga-chapter-img", "src") end
  for _, s in ipairs(srcs) do
    local url = host.text.trim(s)
    if url ~= "" then out[#out + 1] = url end
  end
  if #out == 0 then return { ok = false, error = "mangaread: no pages for " .. composite } end
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
