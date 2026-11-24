-- MangaGoku (www.mangagoku.com): Next.js front end, MangaBox-style JSON API.
-- Ported from tmp/mangakalotgg.har; the same frontend was captured as
-- www.mangakakalot.gg in the same session. Search results come from
-- /home/search/json?searchword=, chapters from /api/manga/<slug>/chapters,
-- and reader images from the chapter HTML's 2xstorage URLs (numeric order).
local SITE = host.plugin.param("site_url") or "https://www.mangagoku.com"
local OPTS = {
  timeout_ms = 45000,
  headers = {
    ["User-Agent"] = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/158.0.0.0 Safari/537.36",
    ["Accept"] = "application/json, text/html, */*",
    ["Referer"] = SITE .. "/",
  },
}

local function http_req(path)
  return host.http.fetch_body(SITE .. path, OPTS)
end

local function get_json(path)
  local body, err = http_req(path)
  if not body or body == "" then return nil, err end
  local ok, doc = pcall(json.decode, body)
  if not ok or type(doc) ~= "table" then return nil, "mangagoku: bad json" end
  return doc
end

local function find1(s, pat)
  return host.re2.capture(pat, tostring(s or ""))
end

local function clean(s)
  return host.text.trim(host.text.unescape(host.text.strip_html(tostring(s or ""))))
end

local function epoch(s)
  return host.text.date_epoch(clean(s)) or 0
end

local function slug_of(id)
  id = tostring(id or "")
  return find1(id, [[/manga/([^/?#]+)]]) or id
end

local function search_page(q, page)
  local doc = get_json("/home/search/json?searchword=" .. host.url_encode(q) .. "&page=" .. page)
  if type(doc) ~= "table" then return nil end
  return doc
end

-- The JSON endpoint has no total/pagination; memoize a bounded binary search
-- over page numbers so the host pager gets a real last page once per query.
local page_memo = {}
local function paging(q)
  local cached = page_memo[q]
  if cached then return cached.total, cached.pages end
  local lo, hi = 1, 2
  if search_page(q, hi) then
    while hi < 256 and search_page(q, hi) do
      lo = hi
      hi = hi * 2
    end
    while lo + 1 < hi do
      local mid = math.floor((lo + hi) / 2)
      if search_page(q, mid) then lo = mid else hi = mid end
    end
  else
    lo = 1
  end
  local last = search_page(q, lo) or {}
  local total = (lo - 1) * 20 + (type(last) == "table" and #last or 0)
  local pages = math.max(1, lo)
  page_memo[q] = { total = total, pages = pages }
  return total, pages
end

local function book_genres(body)
  local raw = find1(body, [==[(?s)"@type":"Book".*?"genre":\[([^\]]*)\]]==]) or ""
  local out = {}
  for name in raw:gmatch('"([^"]+)"') do out[#out + 1] = clean(name) end
  return out
end

local function book_description(body)
  local bs = body:find('"@type":"Book"', 1, true)
  if not bs then return "" end
  local rest = body:sub(bs)
  local at = rest:find('"description":"', 1, true)
  if not at then return "" end
  local start = at + #'"description":"'
  local out, k = {}, start
  while k <= #rest do
    local c = rest:sub(k, k)
    if c == '\\' then
      out[#out + 1] = rest:sub(k, k + 1)
      k = k + 2
    elseif c == '"' then
      break
    else
      out[#out + 1] = c
      k = k + 1
    end
  end
  local raw = table.concat(out)
  local ok, decoded = pcall(json.decode, '"' .. raw .. '"')
  return clean(ok and decoded or raw)
end

methods = {}

methods.search = function(params)
  local q = clean(params.title or params.query or params.q or "")
  local page = math.max(1, tonumber(params.page) or 1)
  if q == "" then return { ok = true, data = { items = {}, total = 0, pages = 1 } } end
  local items_doc = search_page(q, page)
  if not items_doc then return { ok = false, error = "mangagoku: search failed" } end
  local items = {}
  for _, it in ipairs(items_doc) do
    if it.slug and it.slug ~= "" then
      items[#items + 1] = {
        id = it.slug,
        name = clean(it.name),
        cover = it.thumb or "",
        author = clean(it.author or ""),
      }
    end
  end
  local total, pages = paging(q)
  return { ok = true, data = { items = items, total = total, pages = pages } }
end

methods.detail = function(params)
  local id = slug_of(params.id or "")
  if id == "" then return { ok = true, data = {} } end
  local body, err = http_req("/manga/" .. host.url_encode(id))
  if not body or body == "" then return { ok = false, error = err } end

  -- The RSC payload carries several ld+json blobs (Organization first, Book
  -- later); anchor on "@type":"Book" instead of taking the first blob.
  local genres, authors = book_genres(body), {}
  local author = find1(body, [[(?s)"@type":"Book".*?"author":.*?"name":"([^"]+)"]])
  if author then
    author = clean(author)
    local seen = {}
    for _, a in ipairs(authors) do seen[a] = true end
    if not seen[author] then authors[#authors + 1] = author end
  end

  local name = clean(find1(body, [[<meta property="og:title" content="([^"]+)]]) or "")
  name = name:gsub("%s*|%s*MangaGoku%s*$", "")
  local status = clean(find1(body, [[(?s)Status</span><span[^>]*>([^<]+)</span>]]) or "")
  if status == "" then status = clean(find1(body, [[text%-green%-400">([^<]+)</span>]]) or "") end

  return { ok = true, data = {
    id = id,
    name = name,
    cover = find1(body, [[<meta property="og:image" content="([^"]+)]]) or "",
    description = book_description(body),
    status = host.text.normalize_status(status),
    tags = genres,
    authors = authors,
    artists = {},
    release_at = 0,
    url = SITE .. "/manga/" .. id,
  } }
end

methods.chapters = function(params)
  local id = slug_of(params.id or "")
  if id == "" then return { ok = true, data = {} } end
  local doc, err = get_json("/api/manga/" .. host.url_encode(id) .. "/chapters?limit=-1")
  if not doc then return { ok = false, error = err } end
  local out = {}
  for _, c in ipairs((doc.data or {}).chapters or {}) do
    if c.chapter_slug and c.chapter_slug ~= "" then
      out[#out + 1] = {
        id = id .. ":" .. c.chapter_slug,
        number = tonumber(c.chapter_num) or 0,
        title = clean(c.chapter_name or "Chapter"),
        release_at = epoch(c.updated_at),
        pages = 0,
      }
    end
  end
  table.sort(out, function(a, b) return a.number < b.number end)
  return { ok = true, data = out }
end

methods.pages = function(params)
  local composite = tostring(params.id or "")
  local id, chapter = composite:match("^([^:]+):(.+)$")
  if not id or not chapter then return { ok = true, data = {} } end
  local body, err = http_req("/manga/" .. host.url_encode(id) .. "/" .. host.url_encode(chapter))
  if not body or body == "" then return { ok = false, error = err } end
  -- Reader images are 2xstorage URLs with a trailing /<n>.webp index; the CDN
  -- host varies per title (img-r1/img-r2/imgs-2), so match any subdomain. Thumb
  -- images live under /thumb/ and never carry that numeric tail.
  local found = {}
  for url in body:gmatch([[https://[%w%-]+%.2xstorage%.com/[^"\\%s]+/%d+%.webp]]) do
    found[#found + 1] = url
  end
  -- The same page appears on several CDN mirrors; keep one URL per index.
  local by_index, order = {}, {}
  for _, url in ipairs(found) do
    local idx = tonumber(url:match('/([0-9]+)%.webp$'))
    if idx and not by_index[idx] then
      by_index[idx] = url
      order[#order + 1] = idx
    end
  end
  table.sort(order)
  local out = {}
  for _, idx in ipairs(order) do out[#out + 1] = by_index[idx] end
  return { ok = true, data = out }
end

methods.genres = function()
  -- No standalone genre endpoint: read the site-wide catalogue from the
  -- schema.org Book blob of the first search hit.
  local first = search_page("isekai", 1)
  if type(first) ~= "table" or not first[1] or not first[1].slug then
    return { ok = true, data = {} }
  end
  local body = http_req("/manga/" .. host.url_encode(first[1].slug)) or ""
  return { ok = true, data = book_genres(body) }
end
function handle(req)
  local fn = methods[req.method]
  if not fn then return { ok = false, error = "mangagoku: unknown method " .. tostring(req.method) } end
  local ok, res = pcall(fn, req.params or {})
  if not ok then return { ok = false, error = "mangagoku: " .. tostring(res) } end
  return res
end
