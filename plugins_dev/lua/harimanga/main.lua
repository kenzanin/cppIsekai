-- HariManga (harimanga.cc): WordPress Madara theme, HTML scraping; Cloudflare-fronted.
-- Port of goIsekai examples/plugins/lua/harimanga; adapted to cppIsekai host tools
-- (host.http.fetch replaces host.http.get_body/post_body, host.re2.* replaces
-- host.regex.* with arg order pattern,subject). Envelope { ok, data | error };
-- canonical manga fields per the mangadex plugin.
local BASE = "https://www.harimanga.cc" -- single origin; manifest site_url kept verbatim in main.json

-- http request -> (body, err). 200-or-nothing, the anisascans http_req shape.
local http_req = host.http.fetch_body -- (body, err); nil unless 200 (plugin-host-promotion).

-- first capture group of the first match; nil when no match (host re2.capture).
local function find1(s, pattern)
  return host.re2.capture(pattern, tostring(s or ""))
end

local methods = {}

-- name -> slug map filled by methods.genres; search resolves UI genre names.
local genre_map = nil

-- ── search ─────────────────────────────────────────────────────────────────

methods.search = function(params)
  local q = params.title or params.query or params.q or ""
  local page = math.max(1, tonumber(params.page) or 1)
  local genres = params.tags or params.genres or {}
  if type(genres) == "string" then -- UI joins checked genres with commas
    local list = {}
    for name in genres:gmatch("[^,]+") do list[#list + 1] = name:match("^%s*(.-)%s*$") end
    genres = list
  end

  local url
  if type(genres) == "table" and #genres > 0 then
    -- Madara genre archives: /manga-genre/<slug>/, page N as /page/N/.
    local slug = (genre_map and genre_map[genres[1]]) or genres[1]
    url = BASE .. "/manga-genre/" .. host.url_encode(slug) .. "/"
    if page > 1 then url = url .. "page/" .. tostring(page) .. "/" end
  else
    -- Madara GET search: admin-ajax.php answers 0 bytes, use server-rendered GET.
    url = BASE .. "/?s=" .. host.url_encode(q) .. "&post_type=wp-manga"
  end

  local body, err = http_req(url)
  if err ~= nil then return { ok = false, error = err } end

  local items, seen = {}, {}
  -- one card per c-tabs-item__content block: cover <img> first, then post-title
  -- link; capture up to the close so the cover stays in the same card.
  for _, m in ipairs(host.re2.find_all([[(?s)<div[^>]*class="[^"]*c-tabs-item__content.*?post-title.*?</h3>]], body)) do
    local card = m[1]
    local href = find1(card, [[href="([^"]*/manga/[^"/]+/)]])
    if href then
      local slug = find1(href, [[/manga/([^/]+)/]]) or find1(href, [[/manga/([^/]+)$]])
      if slug and not seen[slug] then
        seen[slug] = true
        local cover = find1(card, [[data-src="[\t\n\s]*(https?://[^"]+)"]])
          or find1(card, [[\ssrc="(https?://[^"]+)"]])
        local title = find1(card, [[<h3[^>]*><a[^>]*>([^<]+)</a>]])
        items[#items + 1] = { id = slug, name = host.text.unescape(title or ""), cover = host.text.trim(cover or "") }
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
  local cover = host.text.trim(find1(cover_block, [[data-src="([^"]+)"]]) or find1(cover_block, [[src="([^"]+)"]]) or "")

  local tblock = find1(body, [[(?s)class="post-title[^"]*">(.*?)</div>]]) or ""
  local name = host.text.unescape(host.text.strip_html(find1(tblock, [[(?s)<h[1-6][^>]*>(.*?)</h[1-6]>]]) or ""))

  local author = host.text.strip_html(find1(body, [[(?s)class="author-content">(.*?)</div>]]) or ""):gsub(",", ", ")

  local tags = {}
  local gblock = find1(body, [[(?s)class="genres-content">(.*?)</div>]]) or ""
  for _, g in ipairs(host.re2.find_all([[<a[^>]*>([^<]+)</a>]], gblock)) do
    local tag = host.text.unescape(host.text.trim(g[2]))
    if tag ~= "" then tags[#tags + 1] = tag end
  end

  local status = "unknown"
  local sblock = find1(body, [[(?s)class="post-status">(.*?)$]]) or ""
  if sblock ~= "" then
    local after = find1(sblock, [[(?s)Status\s*</h5>.*?class="summary-content">\s*([^<]+)]])
    if after then status = host.text.normalize_status(host.text.trim(after)) end
  end

  local dblock = find1(body, [[(?s)class="summary__content[^"]*">(.*?)<span\s+class="[^"]*content-readmore"]]) or ""
  if dblock == "" then dblock = find1(body, [[(?s)class="summary__content[^"]*">(.*?)</div>]]) or "" end
  local description = host.text.strip_html(dblock):gsub("%s+", " ")

  local data = empty
  data.name = name
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
          id = slug .. ":" .. cid, -- composite slug:chapterId; pages needs both
          number = host.text.chapter_num(label) or 0,
          title = label,
          release_at = 0, -- Madara dates are human strings; host wants epoch ints (anisascans precedent)
          pages = 0,
        }
      end
    end
  end

  -- fallback: chapter links in the page body (theme variants without the li)
  if #out == 0 then
    for _, m in ipairs(host.re2.find_all([[<a[^>]*href="([^"]*chapter-[^"]+)"[^>]*>\s*([^<]+)\s*</a>]], body)) do
      local href, label = m[2], m[3]
      local cid = href and find1(href, [[/([^/]+)/?$]])
      if cid and not seen[cid] then
        seen[cid] = true
        out[#out + 1] = { id = slug .. ":" .. cid, number = host.text.chapter_num(label) or 0,
          title = host.text.trim(label or ""), release_at = 0, pages = 0 }
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
    return { ok = false, error = "harimanga: chapter id must be slug:chapterId, got " .. tostring(params.id) }
  end
  local body, err = http_req(BASE .. "/manga/" .. slug .. "/" .. cid .. "/")
  if err ~= nil then return { ok = false, error = err } end

  -- Madara page images carry class wp-manga-chapter-img; lazy data-src, eager src.
  local out = {}
  local srcs = host.html.attr(body, "img.wp-manga-chapter-img", "data-src")
  if #srcs == 0 then srcs = host.html.attr(body, "img.wp-manga-chapter-img", "src") end
  for _, s in ipairs(srcs) do
    local url = host.text.trim(s)
    if url ~= "" then out[#out + 1] = url end
  end
  if #out == 0 then return { ok = false, error = "harimanga: no pages for " .. composite } end
  return { ok = true, data = out }
end

-- ── genres ─────────────────────────────────────────────────────────────────

methods.genres = function()
  -- Scraped from the site's own nav (source's choice: a hardcoded list rots
  -- when the theme reorganises). Names for the UI; slugs memoized for search.
  local body, err = http_req(BASE .. "/")
  if err ~= nil then return { ok = false, error = err } end
  local seen, names = {}, {}
  local map = {}
  for _, m in ipairs(host.re2.find_all([[href="[^"]*harimanga\.cc/manga-genre/([a-z0-9-]+)/"[^>]*>([^<]+)</]], body)) do
    local slug, label = m[2], host.text.unescape(host.text.trim(m[3]))
    if label ~= "" and not seen[slug] then
      seen[slug] = true
      names[#names + 1] = label
      map[label] = slug
    end
  end
  table.sort(names)
  genre_map = map
  return { ok = true, data = names }
end

function handle(req)
  local fn = methods[req.method]
  if fn == nil then return { ok = false, error = "unknown method " .. tostring(req.method) } end
  return fn(req.params or {})
end
