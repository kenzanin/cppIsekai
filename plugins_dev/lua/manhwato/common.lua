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



local function detail_body(slug)
  return get("/manhwa/" .. host.url_encode(slug))
end

return { SITE=SITE, get=get, detail_body=detail_body, find1=find1, find_all=find_all, clean=clean, epoch=epoch, cards=cards, last_page=last_page, genre_slug=genre_slug, page_cache=page_cache, page_info=page_info }
