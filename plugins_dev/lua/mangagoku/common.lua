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



return { SITE=SITE, OPTS=OPTS, http_req=http_req, get_json=get_json, find1=find1, clean=clean, epoch=epoch, slug_of=slug_of, search_page=search_page, page_memo=page_memo, paging=paging, book_genres=book_genres, book_description=book_description }
