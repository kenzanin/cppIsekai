-- Asmodeus Scans (asmotoon.com): KeyoappV2 white-label, HTML pages plus two
-- AJAX helpers. Port of goIsekai extension-source/src/en/asmotoon (Asmotoon.kt
-- over lib-multisrc/keyoapp/KeyoappV2.kt). Search = /?_ajax=search&q=&offset=;
-- chapter rows = /?_chapters_html=1&series_slug=; pages = #strip-reader imgs.
local BASE = host.plugin.param("site_url") or "https://asmotoon.com"
local OPTS = {
  timeout_ms = 45000,
  headers = {
    ["User-Agent"] = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/158.0.0.0 Safari/537.36",
    ["Referer"] = BASE .. "/",
    ["X-Requested-With"] = "XMLHttpRequest",
  },
}

local function http_req(url, extra)
  if not extra then return host.http.fetch_body(url, OPTS) end
  local o = {}
  for k, v in pairs(OPTS) do o[k] = v end
  for k, v in pairs(extra) do o[k] = v end
  return host.http.fetch_body(url, o)
end

local function find1(s, pat) return host.re2.capture(pat, tostring(s or "")) end
local function clean(s) return host.text.trim(host.text.unescape(host.text.strip_html(tostring(s or "")))) end
local function first(t) return type(t) == "table" and t[1] or nil end
local function status_of(text)
  local t = tostring(text or ""):lower()
  if t:find("ongoing", 1, true) then return "ongoing" end
  if t:find("completed", 1, true) then return "completed" end
  if t:find("dropped", 1, true) or t:find("cancelled", 1, true) then return "cancelled" end
  if t:find("paused", 1, true) or t:find("hiatus", 1, true) then return "hiatus" end
  return ""
end

local function series_url(slug) return BASE .. "/" .. host.url_encode(slug) end

methods = {}

methods.search = function(params)
  local q = clean(params.title or params.query or params.q or "")
  local page = math.max(1, tonumber(params.page) or 1)
  local limit = 20
  local url = BASE .. "/?_ajax=search&q=" .. host.url_encode(q) .. "&offset=" .. ((page - 1) * limit)
  local body, err = http_req(url)
  if not body or body == "" then return { ok = false, error = err } end
  local ok, doc = pcall(json.decode, body)
  if not ok or type(doc) ~= "table" then return { ok = false, error = "asmodeus: bad search envelope" } end
  local html = doc.html or ""
  local hrefs = host.html.attr(html, "a.group[href]", "href") or {}
  local names = host.html.select(html, "a.group[href] h2") or {}
  local covers = host.html.attr(html, "a.group[href] img", "data-src") or {}
  local items = {}
  for i, href in ipairs(hrefs) do
    local slug = find1(href, [[^/([^/]+)]])
    if slug and slug ~= "" then
      items[#items + 1] = {
        id = slug,
        name = clean(names[i] or ""),
        cover = covers[i] or "",
        url = BASE .. href,
      }
    end
  end
  return { ok = true, data = { items = items, total = #items + (doc.has_more and limit or 0),
                               pages = page + (doc.has_more and 1 or 0) } }
end

methods.detail = function(params)
  local slug = find1(params.id or "", [[^/([^/]+)]]) or tostring(params.id or "")
  local body, err = http_req(series_url(slug))
  if not body or body == "" then return { ok = false, error = err } end
  local by_text = clean(first(host.html.select(body, "p.l2-by")) or "")
  by_text = by_text:gsub("^By%s+", "")
  local author, artist = by_text:match("^([^,]+),(.+)$")
  author = clean(author or by_text)
  artist = clean(artist or "")
  local kicker = clean(first(host.html.select(body, "p.l2-kicker")) or "")
  if kicker == "" then kicker = clean(first(host.html.select(body, "div.l2-meta span")) or "") end
  local tags = {}
  for _, g in ipairs(host.html.select(body, "a[href*='tag%5B%5D']") or {}) do
    g = clean(g)
    if g ~= "" then tags[#tags + 1] = g end
  end
  local desc = clean(first(host.html.attr(body, "meta[property='og:description']", "content")) or "")
  return { ok = true, data = {
    id = slug,
    name = clean(first(host.html.select(body, "h1.l2-title")) or ""),
    cover = first(host.html.attr(body, "meta[property='og:image']", "content")) or "",
    description = desc,
    status = host.text.normalize_status(status_of(kicker)),
    tags = tags,
    authors = author ~= "" and { author } or {},
    artists = artist ~= "" and { artist } or {},
    url = series_url(slug),
  } }
end

methods.chapters = function(params)
  local slug = find1(params.id or "", [[^/([^/]+)]]) or tostring(params.id or "")
  local body, err = http_req(BASE .. "/?_chapters_html=1&series_slug=" .. host.url_encode(slug))
  if not body or body == "" then return { ok = false, error = err } end
  local ok, doc = pcall(json.decode, body)
  if not ok or type(doc) ~= "table" then return { ok = false, error = "asmodeus: bad chapters envelope" } end
  local html = doc.rows_html or ""
  local hrefs = host.html.attr(html, "a.chapter-row[href]", "href") or {}
  local titles = host.html.attr(html, "a.chapter-row[href]", "data-title") or {}
  local out = {}
  for i, href in ipairs(hrefs) do
    local num = host.text.chapter_num(titles[i] or href) or 0
    out[#out + 1] = {
      id = slug .. ":" .. find1(href, [[^/([^/]+)]]),
      number = num,
      title = clean(titles[i] or ("Chapter " .. tostring(num))),
      release_at = 0,
      pages = 0,
      url = BASE .. href,
    }
  end
  table.sort(out, function(a, b) return a.number < b.number end)
  return { ok = true, data = out }
end

methods.pages = function(params)
  local composite = tostring(params.id or "")
  local slug, chapter = composite:match("^([^:]+):(.+)$")
  if not slug or not chapter then return { ok = true, data = {} } end
  local body, err = http_req(BASE .. "/" .. host.url_encode(chapter))
  if not body or body == "" then return { ok = false, error = err } end
  local urls = host.html.attr(body, "#strip-reader img.reader-page", "src") or {}
  if #urls == 0 then urls = host.html.attr(body, "#strip-reader img.reader-page", "data-src") or {} end
  local out = {}
  for _, u in ipairs(urls) do
    u = clean(u)
    if u ~= "" then out[#out + 1] = host.url_abs(BASE, u) end
  end
  return { ok = true, data = out }
end

methods.genres = function()
  local body = http_req(BASE .. "/?browse=1") or ""
  local out, seen = {}, {}
  for _, g in ipairs(host.html.select(body, "#fsel-genres a[role=option][href]") or {}) do
    g = clean(g)
    if g ~= "" and not seen[g] then seen[g] = true; out[#out + 1] = g end
  end
  return { ok = true, data = out }
end

function handle(req)
  local fn = methods[req.method]
  if not fn then return { ok = false, error = "asmodeus: unknown method " .. tostring(req.method) } end
  local ok, res = pcall(fn, req.params or {})
  if not ok then return { ok = false, error = "asmodeus: " .. tostring(res) } end
  return res
end
