-- Manhwato (manhwato.com): server-rendered HTML, ported from tmp/manhwato.har
-- and goIsekai's wasm example. Search is GET /search?q=&page=N (30 cards);
-- genre chips use /manhwa-genre/<slug>?page=N. Detail carries the chapter list,
-- and a chapter page holds <figure class="mt-page"><img src=...> in order.
local SITE = "https://manhwato.com"
-- The site's TLS chain is rejected by the linked curl-impersonate (BoringSSL)
-- while plain system curl in this process would segfault alongside it, so all
-- pages go through the daemon's own Chrome via the shared host.browser bridge.
local function get(path)
  local _, gerr = host.browser.call("goto", { url = SITE .. path, timeout_ms = 45000 })
  if gerr then return nil, gerr end
  local body, eerr = host.browser.call("evaluate", {
    script = "document.documentElement.outerHTML", timeout_ms = 45000,
  })
  if not body or body == "" then return nil, eerr or "empty document" end
  return body
end

local function find1(s, pat)
  return host.re2.capture(pat, tostring(s or ""))
end

local function find_all(s, pat)
  local ok, res = pcall(host.re2.find_all, pat, tostring(s or ""))
  if not ok then return {} end
  return res or {}
end

local function clean(s)
  return host.text.trim(host.text.unescape(host.text.strip_html(tostring(s or ""))))
end

local function epoch(s)
  return host.text.date_epoch(clean(s)) or 0
end

local function cards(body)
  local out = {}
  for _, m in ipairs(find_all(body, [[(?s)<article class="mt-update">(.*?)</article>]]) ) do
    local block = m[2]
    local slug = find1(block, [[<a class="mt-update__cover" href="/manhwa/([a-z0-9-]+)"]])
    local cover = find1(block, [[<img src="([^"]+)" alt="[^"]*"]])
    if slug then
      out[#out + 1] = {
        id = slug,
        name = clean(find1(block, [[<h2 class="mt-update__title"><a href="[^"]+" title="([^"]*)"]]) or slug),
        cover = cover or "",
        url = SITE .. "/manhwa/" .. slug,
      }
    end
  end
  return out
end

local function last_page(body)
  local max = 1
  for _, m in ipairs(find_all(body, [[aria-label="Page (\d+)"]]) ) do
    local n = tonumber(m[2]) or 1
    if n > max then max = n end
  end
  return max
end

local genre_map
local function genre_slug(name)
  if not genre_map then
    genre_map = {}
    local body = get("/manhwa-list")
    for _, m in ipairs(find_all(body or "", [[href="/manhwa-genre/([a-z0-9-]+)"[^>]*>([^<]+)</a>]]) ) do
      genre_map[clean(m[3]):lower()] = m[2]
    end
  end
  return genre_map[clean(name):lower()]
end

-- The last SERP page has fewer cards; fetch it once per query and memoize so the
-- host pager gets a real total instead of one-page-worth of items.
local page_cache = {}
local function page_info(key, path, first_body, first_count)
  local hit = page_cache[key]
  if hit then return hit end
  local sep = path:find("?", 1, true) and "&" or "?"
  -- The pager omits the current page link, so read it from page 1 (fetched
  -- above) rather than from whichever page the caller requested.
  local body = first_body
  if last_page(body or "") == 1 and first_count > 0 then
    body = get(path)
  end
  local pages = last_page(body or "")
  local last = first_count
  if pages > 1 then
    local tail = get(path .. sep .. "page=" .. pages)
    last = #cards(tail or "")
  end
  hit = { pages = pages, total = (pages - 1) * 30 + last }
  page_cache[key] = hit
  return hit
end

methods = {}

methods.search = function(params)
  local q = clean(params.title or params.query or params.q)
  local page = math.max(1, tonumber(params.page) or 1)
  local tags = params.tags or params.genres
  local key = q:lower()
  local path
  if type(tags) == "table" and #tags > 0 then
    local slug = genre_slug(tostring(tags[1]))
    if not slug then return { ok = true, data = { items = {}, total = 0, pages = 1 } } end
    path = "/manhwa-genre/" .. slug
    key = key .. "|" .. slug
  elseif type(tags) == "string" and tags ~= "" then
    local slug = genre_slug(tags)
    if not slug then return { ok = true, data = { items = {}, total = 0, pages = 1 } } end
    path = "/manhwa-genre/" .. slug
    key = key .. "|" .. slug
  else
    path = "/search?q=" .. host.url_encode(q)
  end
  local body, err = get(path .. (page > 1 and ("&page=" .. page) or ""))
  if not body then return { ok = false, error = err or "manhwato: search failed" } end
  local items = cards(body)
  local info = page_info(key, path, body, #items)
  return { ok = true, data = { items = items, total = info.total, pages = info.pages } }
end

local function detail_body(slug)
  return get("/manhwa/" .. host.url_encode(slug))
end

methods.detail = function(params)
  local slug = tostring(params.id or "")
  if slug == "" then return { ok = true, data = {} } end
  local body, err = detail_body(slug)
  if not body then return { ok = false, error = err or "manhwato: detail failed" } end
  return { ok = true, data = {
    id = slug,
    name = clean(find1(body, [[<h1 class="mt-dhero__title">([^<]+)]]) or slug),
    cover = find1(body, [[<meta property="og:image" content="([^"]+)"]]) or "",
    description = clean(find1(body, [[(?s)<div class="about">(.*?)</div>]])),
    status = host.text.normalize_status(clean(find1(body, [[<dt>Status</dt><dd><span[^>]*></span>([^<]+)]]) or "")),
    authors = { clean(find1(body, [[<dt>Author</dt><dd><a[^>]*>([^<]+)]]) or "") },
    tags = (function()
      local out = {}
      local block = find1(body, [[(?s)<ul class="mt-pills mt-pills--muted">(.*?)</ul>]]) or ""
      for _, m in ipairs(find_all(block, [[<a[^>]*>([^<]+)</a>]]) ) do
        out[#out + 1] = clean(m[2])
      end
      return out
    end)(),
    genres = (function()
      local out = {}
      for name in tostring(find1(body, "(?s)\"genre\":\\s*\\[(.*?)\\]") or ""):gmatch("\"([^\"]+)\"") do
        out[#out + 1] = clean(name)
      end
      return out
    end)(),
    url = SITE .. "/manhwa/" .. slug,
  } }
end

methods.chapters = function(params)
  local slug = tostring(params.id or "")
  if slug == "" then return { ok = true, data = {} } end
  local body, err = detail_body(slug)
  if not body then return { ok = false, error = err or "manhwato: chapters failed" } end
  local out = {}
  for _, m in ipairs(find_all(body, [[(?s)<li data-ch="\d+".*?<a href="/manhwa/[^"]+/chapter-([\d.]+)-ch(\d+)" title="([^"]*)"]]) ) do
    local num, chid, title = tonumber(m[2]) or 0, m[3], clean(m[4])
    if chid then
      out[#out + 1] = {
        id = slug .. ":" .. chid,
        number = num,
        title = title,
        release_at = epoch(find1(body, [[(?s)chapter-]] .. tostring(num) .. [[-ch]] .. chid .. [[[^>]*>.*?<time datetime="([^"]+)"]])),
        pages = 0,
        url = SITE .. "/manhwa/" .. slug .. "/chapter-" .. tostring(num) .. "-ch" .. chid,
      }
    end
  end
  table.sort(out, function(a, b) return a.number < b.number end)
  return { ok = true, data = out }
end

methods.pages = function(params)
  local composite = tostring(params.id or "")
  local slug, chid = composite:match("^([^:]+):(.+)$")
  if not slug or not chid then return { ok = true, data = {} } end
  local detail = detail_body(slug)
  if not detail then return { ok = true, data = {} } end
  local href = find1(detail, [[href="(/manhwa/[^"]*-ch]] .. chid .. [[)"]])
  if not href then return { ok = true, data = {} } end
  local body, err = get(href)
  if not body then return { ok = false, error = err or "manhwato: pages failed" } end
  local out = {}
  for _, m in ipairs(find_all(body, [[(?s)<figure class="mt-page".*?<img src="([^"]+)"]]) ) do
    if m[2] and m[2] ~= "" then out[#out + 1] = m[2] end
  end
  return { ok = true, data = out }
end

methods.genres = function()
  local body, err = get("/manhwa-list")
  if not body then return { ok = false, error = err or "manhwato: genres failed" } end
  local out = {}
  for _, m in ipairs(find_all(body, [[href="/manhwa-genre/([a-z0-9-]+)"[^>]*>([^<]+)</a>]]) ) do
    out[#out + 1] = clean(m[3])
  end
  return { ok = true, data = out }
end

function handle(req)
  local fn = methods[req.method]
  if not fn then return { ok = false, error = "manhwato: unknown method " .. tostring(req.method) } end
  local ok, res = pcall(fn, req.params or {})
  if not ok then return { ok = false, error = "manhwato: " .. tostring(res) } end
  return res
end
