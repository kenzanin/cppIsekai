-- AnisaScan plugin (anisascans.in): WordPress Madara theme, HTML scraping.
-- Port of goIsekai examples/plugins/lua/anisascans; adapted to cppIsekai host tools
-- (host.http.fetch replaces goIsekai host.http.get_body; host.re2.find replaces
-- host.regex.find; see references/goisekai-abi-mapping for the full gap list).
-- Envelope { ok, data | error }; canonical manga fields per the mangadex plugin.
local BASE = "https://anisascans.com" -- .in died (NXDOMAIN); .com is live

-- ── helpers ────────────────────────────────────────────────────────────────

local function trim(s)
  return (tostring(s or ""):gsub("^%s+", ""):gsub("%s+$", ""))
end

local function unescape(s)
  s = tostring(s or "")
  s = s:gsub("&amp;", "&"):gsub("&lt;", "<"):gsub("&gt;", ">"):gsub("&quot;", '"'):gsub("&#39;", "'"):gsub("&nbsp;", " ")
  s = s:gsub("&#(%d+);", function(n) return string.char(tonumber(n)) end)
  s = s:gsub("&#x(%x+);", function(h) return string.char(tonumber(h, 16)) end)
  return s
end

local function strip_html(s)
  return trim(unescape(tostring(s or ""):gsub("<[^>]*>", " ")))
end

-- canonical status; unknown passes through, empty -> "unknown"
local function normalize_status(s)
  s = trim(tostring(s or ""):lower():gsub("%s+", " "))
  if s == "" then return "unknown" end
  if s == "ongoing" or s == "releasing" or s == "publishing" then return "Ongoing" end
  if s == "completed" or s == "finished" then return "Completed" end
  if s == "hiatus" or s == "on hiatus" then return "Hiatus" end
  if s == "dropped" or s == "cancelled" then return "Dropped" end
  if s == "upcoming" or s == "not yet released" then return "Upcoming" end
  return s
end

-- Madara labels ("Chapter 12", "Ch. 12") -> number; 0 when none.
local function chapter_num(s)
  s = tostring(s or "")
  local n = s:match("(%d+%.?%d*)%s*$")
  if n then return tonumber(n) end
  n = s:match("[Cc]hapter%s+(%d+%.?%d*)")
  if n then return tonumber(n) end
  return 0
end

-- http request -> (body, err). cppIsekai host.http.fetch replaces goIsekai get_body.
local function http_req(url, opts)
  local res, err = host.http.fetch(url, opts or {})
  if err ~= nil then return nil, err end
  if res.status ~= 200 then return nil, "anisascans: upstream status " .. tostring(res.status) end
  return res.body, nil
end

-- every match of `pattern` in `s`, each as a re2 group array (goIsekai regex.gmatch).
local function find_all(s, pattern)
  local out, pos = {}, 1
  s = tostring(s or "")
  while pos <= #s do
    local m = host.re2.find(pattern, s:sub(pos))
    if #m == 0 or m[1] == "" then break end
    out[#out + 1] = m
    pos = pos + #m[1]
  end
  return out
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
    for name in genres:gmatch("[^,]+") do list[#list + 1] = trim(name) end
    genres = list
  end

  local url
  if type(genres) == "table" and #genres > 0 then
    -- Madara genre archives live under /manga-genre/<slug>/; ?page=N is not
    -- supported by this theme, so page 2+ uses /page/N/.
    url = BASE .. "/manga-genre/" .. host.url_encode(genres[1]) .. "/"
    if page > 1 then url = url .. "page/" .. tostring(page) .. "/" end
  else
    -- Madara GET search: admin-ajax.php answers 0 bytes, use server-rendered GET.
    url = BASE .. "/?s=" .. host.url_encode(q) .. "&post_type=wp-manga"
  end

  local body, err = http_req(url)
  if err ~= nil then return { ok = false, error = err } end

  local items, seen = {}, {}
  -- one card per c-tabs-item__content block: cover <img> first, then post-title
  -- link. Capture up to the post-title close so the cover stays in the same card.
  for _, m in ipairs(find_all(body, [[(?s)<div[^>]*class="[^"]*c-tabs-item__content.*?post-title.*?</h3>]])) do
    local card = m[1]
    local href = find1(card, [[href="([^"]*/manga/[^"/]+/)"]])
    local title = find1(card, [[<h3[^>]*><a[^>]*>([^<]+)</a>]])
    if href then
      local slug = find1(href, [[/manga/([^/]+)/]]) or find1(href, [[/manga/([^/]+)$]])
      if slug and not seen[slug] then
        seen[slug] = true
        local cover = find1(card, [[data-src="[\t\n\s]*(https?://[^"]+)"]])
          or find1(card, [[\ssrc="(https?://[^"]+)"]])
        items[#items + 1] = { id = slug, name = unescape(title or ""), cover = trim(cover or "") }
      end
    end
  end
  return { ok = true, data = { items = items, total = #items } }
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
  local cover = trim(find1(cover_block, [[data-src="([^"]+)"]]) or find1(cover_block, [[src="([^"]+)"]]) or "")

  local tblock = find1(body, [[(?s)class="post-title[^"]*">(.*?)</div>]]) or ""
  local title = unescape(strip_html(find1(tblock, [[(?s)<h[1-6][^>]*>(.*?)</h[1-6]>]]) or ""))

  local ablock = find1(body, [[(?s)class="author-content">(.*?)</div>]]) or ""
  local author = strip_html(ablock):gsub(",", ", ")

  local gblock = find1(body, [[(?s)class="genres-content">(.*?)</div>]]) or ""
  local tags = {}
  for _, g in ipairs(find_all(gblock, [[<a[^>]*>([^<]+)</a>]])) do
    local name = unescape(trim(g[1]))
    if name ~= "" then tags[#tags + 1] = name end
  end

  local status = "unknown"
  local sblock = find1(body, [[(?s)class="post-status">(.*?)$]]) or ""
  if sblock ~= "" then
    local after = find1(sblock, [[(?s)Status\s*</h5>.*?class="summary-content">\s*([^<]+)]])
    if after then status = normalize_status(after) end
  end

  local dblock = find1(body, [[(?s)class="summary__content[^"]*">(.*?)<span\s+class="[^"]*content-readmore"]]) or ""
  if dblock == "" then dblock = find1(body, [[(?s)class="summary__content[^"]*">(.*?)</div>]]) or "" end
  local description = strip_html(dblock):gsub("%s+", " ")

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
  for _, m in ipairs(find_all(body, [[(?s)<li[^>]*class="wp-manga-chapter[^"]*"[^>]*>(.*?)</li>]])) do
    local li = m[1]
    local href = find1(li, [[href="([^"]+)"]])
    local label = trim(find1(li, [[>([^<]*[Cc]hapter[^<]*)<]]) or "")
    if href and label ~= "" then
      local cid = find1(href, [[/manga/[^/]+/([^/]+)/?$]]) or find1(href, [[/([^/]+)/?$]])
      if cid and not seen[cid] then
        seen[cid] = true
        out[#out + 1] = {
          id = slug .. ":" .. cid, -- composite slug:chapterId, pages needs both
          number = chapter_num(label),
          title = label,
          release_at = 0, -- Madara dates are human strings; host has no epoch contract
          pages = 0,
        }
      end
    end
  end

  -- fallback: chapter links in the page body
  if #out == 0 then
    for _, m in ipairs(find_all(body, [[(?s)<a[^>]*href="([^"]*chapter-[^"]+)"[^>]*>\s*([^<]+)\s*</a>]])) do
      local href, label = m[1], m[2]
      local cid = find1(href, [[/([^/]+)/?$]])
      if cid and not seen[cid] then
        seen[cid] = true
        out[#out + 1] = { id = slug .. ":" .. cid, number = chapter_num(label), title = trim(label), release_at = 0, pages = 0 }
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
    return { ok = false, error = "anisascans: chapter id must be slug:chapterId, got " .. tostring(params.id) }
  end
  local body, err = http_req(BASE .. "/manga/" .. slug .. "/" .. cid .. "/")
  if err ~= nil then return { ok = false, error = err } end

  -- Madara page images carry class wp-manga-chapter-img; lazy themes put the
  -- URL in data-src, eager ones in src.
  local out = {}
  local srcs = host.html.attr(body, "img.wp-manga-chapter-img", "data-src")
  if #srcs == 0 then srcs = host.html.attr(body, "img.wp-manga-chapter-img", "src") end
  for _, s in ipairs(srcs) do
    local url = trim(s)
    if url ~= "" then out[#out + 1] = url end
  end
  if #out == 0 then return { ok = false, error = "anisascans: no pages for " .. composite } end
  return { ok = true, data = out }
end

-- ── genres ─────────────────────────────────────────────────────────────────

methods.genres = function()
  local names = require("genres").names
  return { ok = true, data = names }
end

function handle(req)
  local fn = methods[req.method]
  if fn == nil then return { ok = false, error = "unknown method " .. tostring(req.method) } end
  return fn(req.params or {})
end
