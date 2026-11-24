-- MangaForFree.net: WordPress Madara theme, AdminAjax chapter mode.
-- Port of goIsekai extension-source/src/all/mangaforfree (MangaForFree.kt).
-- Search/detail are plain HTML; chapters come from admin-ajax.php
-- action=manga_get_chapters&manga=<post id>; reader images are plain src on
-- img.wp-manga-chapter-img. Raw chapters are skipped for the en catalogue.
local BASE = host.plugin.param("site_url") or "https://mangaforfree.net"
local AJAX = BASE .. "/wp-admin/admin-ajax.php"
local OPTS = {
  timeout_ms = 45000,
  headers = {
    ["User-Agent"] = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/158.0.0.0 Safari/537.36",
    ["Referer"] = BASE .. "/",
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

local function manga_url(slug) return BASE .. "/manga/" .. host.url_encode(slug) .. "/" end

local function parse_cards(body)
  local hrefs = host.html.attr(body, "a[href*='/manga/'][title]", "href")
  local titles = host.html.attr(body, "a[href*='/manga/'][title]", "title")
  local covers = host.html.attr(body, "a[href*='/manga/'][title] img", "src")
  local out = {}
  for i, href in ipairs(hrefs) do
    local slug = find1(href, [[/manga/([^/]+)]])
    if slug and slug ~= "" and not slug:find("^%?") then
      out[#out + 1] = {
        id = slug,
        name = clean(titles[i] or ""),
        cover = covers[i] or "",
        url = href,
      }
    end
  end
  local seen, uniq = {}, {}
  for _, it in ipairs(out) do
    if not seen[it.id] then seen[it.id] = true; uniq[#uniq + 1] = it end
  end
  return uniq
end

methods = {}

methods.search = function(params)
  local q = clean(params.title or params.query or params.q or "")
  local page = math.max(1, tonumber(params.page) or 1)
  local url = BASE .. "/?s=" .. host.url_encode(q) .. "&post_type=wp-manga"
  if page > 1 then url = url .. "&paged=" .. page end
  local body, err = http_req(url)
  if not body or body == "" then return { ok = false, error = err } end
  return { ok = true, data = { items = parse_cards(body) } }
end

methods.detail = function(params)
  local id = find1(params.id or "", [[/manga/([^/]+)]]) or tostring(params.id or "")
  local body, err = http_req(manga_url(id))
  if not body or body == "" then return { ok = false, error = err } end
  local blob = host.text.json_blob(body, "application/ld+json") or ""
  local ok, ld = pcall(json.decode, blob)
  local doc = ok and type(ld) == "table" and ld or {}
  local tags = {}
  for _, g in ipairs(host.html.select(body, ".genres-content a") or {}) do
    g = clean(g)
    if g ~= "" then tags[#tags + 1] = g end
  end
  -- The page has several "summary-content" blocks (Release, Status, Author),
  -- so anchor on labels instead of taking the first one.
  -- Release is block 1, Status is block 2 in the Madara summary panel.
  local blocks = host.html.select(body, ".post-status .summary-content") or {}
  local status = clean(blocks[2] or "")
  return { ok = true, data = {
    id = id,
    name = clean(first(host.html.attr(body, "meta[property='og:title']", "content")) or doc.name or ""),
    cover = first(host.html.attr(body, "meta[property='og:image']", "content")) or "",
    description = clean(first(host.html.select(body, ".description-summary .summary-content p")) or doc.description or ""),
    status = host.text.normalize_status(status),
    tags = tags,
    authors = clean(first(host.html.select(body, ".author-content a")) or ""),
    artists = {},
    url = manga_url(id),
  } }
end

methods.chapters = function(params)
  local id = find1(params.id or "", [[/manga/([^/]+)]]) or tostring(params.id or "")
  local detail, err = http_req(manga_url(id))
  if not detail or detail == "" then return { ok = false, error = err } end
  local post_id = find1(detail, [["manga_id":"([0-9]+)"]])
  if not post_id then return { ok = true, data = {} } end
  local body = http_req(AJAX, { method = "POST",
    body = "action=manga_get_chapters&manga=" .. post_id,
    headers = { ["Content-Type"] = "application/x-www-form-urlencoded", ["X-Requested-With"] = "XMLHttpRequest" },
  }) or ""
  local hrefs = host.html.attr(body, "li.wp-manga-chapter a[href]", "href") or {}
  local labels = host.html.select(body, "li.wp-manga-chapter a[href]") or {}
  local dates = host.html.select(body, "li.wp-manga-chapter .chapter-release-date i") or {}
  local out = {}
  for i, href in ipairs(hrefs) do
    local title = clean(labels[i] or "")
    if not title:lower():find("raw", 1, true) then
      local slug = find1(href, [[/manga/[^/]+/([^/]+)]])
      if slug then
        out[#out + 1] = {
          id = id .. ":" .. slug,
          number = host.text.chapter_num(title) or 0,
          title = title,
          release_at = host.text.date_epoch(clean(dates[i] or "")) or 0,
          pages = 0,
          url = href,
        }
      end
    end
  end
  table.sort(out, function(a, b) return a.number < b.number end)
  return { ok = true, data = out }
end

methods.pages = function(params)
  local composite = tostring(params.id or "")
  local id, slug = composite:match("^([^:]+):(.+)$")
  if not id or not slug then return { ok = true, data = {} } end
  local body, err = http_req(manga_url(id) .. host.url_encode(slug) .. "/")
  if not body or body == "" then return { ok = false, error = err } end
  local urls = host.html.attr(body, "img.wp-manga-chapter-img", "src") or {}
  if #urls == 0 then urls = host.html.attr(body, "img.wp-manga-chapter-img", "data-src") or {} end
  local out = {}
  for _, u in ipairs(urls) do
    u = clean(u)
    if u ~= "" then out[#out + 1] = host.url_abs(BASE, u) end
  end
  return { ok = true, data = out }
end

methods.genres = function()
  local body = http_req(BASE .. "/manga/?m_orderby=views") or ""
  local out, seen = {}, {}
  for _, g in ipairs(host.html.select(body, ".genres a") or {}) do
    g = clean(g)
    if g ~= "" and not seen[g] then seen[g] = true; out[#out + 1] = g end
  end
  return { ok = true, data = out }
end

function handle(req)
  local fn = methods[req.method]
  if not fn then return { ok = false, error = "mangaforfree: unknown method " .. tostring(req.method) } end
  local ok, res = pcall(fn, req.params or {})
  if not ok then return { ok = false, error = "mangaforfree: " .. tostring(res) } end
  return res
end
