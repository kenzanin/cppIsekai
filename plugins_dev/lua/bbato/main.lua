-- Bbato (bato1.co.uk): HTML cards + JSON chapter list, port of goIsekai
-- extension-source/src/en/bbato (Bbato.kt). Search = /filter?keyword=,
-- detail = /manga/<slug>, chapters = /get-chapter-list?slug=<slug> (XHR),
-- pages = .pages img[data-number] on /read/<slug>/<chapter>.
local BASE = host.plugin.param("site_url") or "https://bato1.co.uk"
local OPTS = {
  timeout_ms = 45000,
  headers = {
    ["User-Agent"] = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/158.0.0.0 Safari/537.36",
    ["Referer"] = BASE .. "/",
  },
}
local XHR = { ["X-Requested-With"] = "XMLHttpRequest" }

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
local function manga_url(slug) return BASE .. "/manga/" .. host.url_encode(slug) end
local function slug_of(id) return find1(id, [[/manga/([^/?#]+)]]) or tostring(id or "") end

local function parse_cards(body)
  local hrefs = host.html.attr(body, ".mt-updates-grid .mt-card a.mt-card-cover", "href") or {}
  local names = host.html.select(body, ".mt-updates-grid .mt-card .mt-card-title") or {}
  local covers = host.html.attr(body, ".mt-updates-grid .mt-card a.mt-card-cover img", "data-src") or {}
  if #covers == 0 then covers = host.html.attr(body, ".mt-updates-grid .mt-card a.mt-card-cover img", "src") or {} end
  local out = {}
  for i, href in ipairs(hrefs) do
    local slug = find1(href, [[/manga/([^/?#]+)]])
    if slug then
      out[#out + 1] = { id = slug, name = clean(names[i] or ""), cover = covers[i] or "", url = href }
    end
  end
  return out
end

methods = {}

methods.search = function(params)
  local q = clean(params.title or params.query or params.q or "")
  local page = math.max(1, tonumber(params.page) or 1)
  local url = BASE .. "/filter?keyword=" .. host.url_encode(q)
  if page > 1 then url = url .. "&page=" .. page end
  local body, err = http_req(url)
  if not body or body == "" then return { ok = false, error = err } end
  local has_next = #host.html.attr(body, ".pagination a[rel=next]", "href") > 0
  return { ok = true, data = { items = parse_cards(body), pages = page + (has_next and 1 or 0) } }
end

methods.detail = function(params)
  local slug = slug_of(params.id or "")
  local body, err = http_req(manga_url(slug))
  if not body or body == "" then return { ok = false, error = err } end
  local tags, authors = {}, {}
  for _, g in ipairs(host.html.select(body, ".mt-comic-badges a, .mt-hero-genres a") or {}) do
    g = clean(g)
    if g ~= "" then tags[#tags + 1] = g end
  end
  for _, a in ipairs(host.html.select(body, ".mt-author-line a") or {}) do
    a = clean(a)
    if a ~= "" then authors[#authors + 1] = a end
  end
  local cover = first(host.html.attr(body, ".mt-comic-poster img", "data-src") or {})
             or first(host.html.attr(body, ".mt-comic-poster img", "src") or "") or ""
  return { ok = true, data = {
    id = slug,
    name = clean(first(host.html.select(body, "h1[itemprop=name]")) or ""),
    cover = cover,
    description = clean(first(host.html.select(body, "#synopsisModal p, .mt-comic-synopsis p")) or ""),
    status = host.text.normalize_status(clean(first(host.html.select(body, ".mt-status-tag")) or "")),
    tags = tags,
    authors = authors,
    artists = {},
    url = manga_url(slug),
  } }
end

methods.chapters = function(params)
  local slug = slug_of(params.id or "")
  local body, err = http_req(BASE .. "/get-chapter-list?slug=" .. host.url_encode(slug), XHR)
  if not body or body == "" then return { ok = false, error = err } end
  local ok, doc = pcall(json.decode, body)
  if not ok or type(doc) ~= "table" or doc.success ~= true then
    return { ok = false, error = "bbato: bad chapters envelope" }
  end
  local out = {}
  for _, c in ipairs(doc.data or {}) do
    if c.chapter_slug and c.chapter_slug ~= "" then
      out[#out + 1] = {
        id = slug .. ":" .. c.chapter_slug,
        number = tonumber(c.chapter_num) or 0,
        title = clean(c.chapter_name or "Chapter"),
        release_at = host.text.date_epoch(tostring(c.updated_at or "")) or 0,
        pages = 0,
      }
    end
  end
  table.sort(out, function(a, b) return a.number < b.number end)
  return { ok = true, data = out }
end

methods.pages = function(params)
  local composite = tostring(params.id or "")
  local slug, chapter = composite:match("^([^:]+):(.+)$")
  if not slug or not chapter then return { ok = true, data = {} } end
  local body, err = http_req(BASE .. "/read/" .. host.url_encode(slug) .. "/" .. host.url_encode(chapter))
  if not body or body == "" then return { ok = false, error = err } end
  -- Page images carry data-number; only the first ones ship a real src and the
  -- rest are lazy (inline SVG placeholder in src, real URL in data-src). The
  -- "do not repost" notice image (data-number='notice') rides along under
  -- /assets/images/, so read attr in order and drop placeholders/notice.
  local function usable(u) return u ~= "" and not u:match("^data:") and not u:find("/assets/images/", 1, true) end
  local urls = {}
  for _, attr in ipairs({ "data-fallback", "data-src", "src" }) do
    local list = {}
    for _, u in ipairs(host.html.attr(body, ".pages img[data-number]", attr) or {}) do
      u = clean(u)
      if usable(u) then list[#list + 1] = host.url_abs(BASE, u) end
    end
    if #list > 0 then urls = list; break end
  end
  local out = {}
  for _, u in ipairs(urls) do
    if u ~= "" and not u:find("notice", 1, true) then out[#out + 1] = u end
  end
  return { ok = true, data = out }
end

methods.genres = function()
  local body = http_req(BASE .. "/filter") or ""
  local out, seen = {}, {}
  for _, g in ipairs(host.html.select(body, ".mt-genre-chip, .genre-list a") or {}) do
    g = clean(g)
    if g ~= "" and not seen[g] and not g:match("^%d+$") then seen[g] = true; out[#out + 1] = g end
  end
  return { ok = true, data = out }
end

function handle(req)
  local fn = methods[req.method]
  if not fn then return { ok = false, error = "bbato: unknown method " .. tostring(req.method) } end
  local ok, res = pcall(fn, req.params or {})
  if not ok then return { ok = false, error = "bbato: " .. tostring(res) } end
  return res
end
