-- MangaDE (mangade.io): public JSON API at api.mangade.io, ported from
-- goIsekai's src/en/mangade plus tmp/mangade.har. Every reply is
-- {"success":true,"data":...}; no HTML scraping, no crypto, no cookies.
-- Search is page-based (20/site page); detail carries news_chapters, and a
-- chapter's images come from /chapters/<chapterId>/view (comic+slug also work).
local API = host.plugin.param("api") or "https://api.mangade.io/api"
local SITE = host.plugin.param("site_url") or "https://mangade.io"
local OPTS = {
  timeout_ms = 45000,
  accept = "application/json, text/plain, */*",
  headers = { Referer = SITE .. "/", Origin = SITE },
}

local function get_json(path)
  local body, err = host.http.fetch_body(API .. path, OPTS)
  if not body or body == "" then return nil, err end
  local ok, doc = pcall(json.decode, body)
  if not ok or type(doc) ~= "table" or doc.success ~= true or type(doc.data) ~= "table" then
    return nil, "mangade: bad envelope"
  end
  return doc.data
end

local function find1(s, pat)
  return host.re2.capture(pat, tostring(s or ""))
end

local function clean(s)
  return host.text.trim(host.text.unescape(tostring(s or "")))
end

local function epoch(s)
  return host.text.date_epoch(clean(s)) or 0
end

local function split_names(s)
  local out = {}
  for raw in tostring(s or ""):gmatch("[^,]+") do
    local name = clean(raw)
    if name ~= "" then out[#out + 1] = name end
  end
  return out
end

local function names(list)
  local out = {}
  if type(list) ~= "table" then return out end
  for _, v in ipairs(list) do
    local name = type(v) == "table" and (v.name or v.title or v.slug) or v
    name = clean(name)
    if name ~= "" then out[#out + 1] = name end
  end
  return out
end

-- The host passes the numeric comic id; tolerate the site's /comic/<slug>-pid<id>
-- URL too, so a pasted link works as an id.
local function comic_id(v)
  local s = tostring(v or "")
  return find1(s, "pid(%d+)") or s
end

methods = {}

local genre_map
local function genre_ids(tags)
  if type(tags) == "string" then
    local list = {}
    for name in tags:gmatch("[^,]+") do list[#list + 1] = clean(name) end
    tags = list
  end
  if type(tags) ~= "table" or #tags == 0 then return {} end
  if not genre_map then
    genre_map = {}
    local data = get_json("/genres?size=500")
    for _, g in ipairs((data and data.genres) or {}) do
      if g.id then
        genre_map[clean(g.name):lower()] = tostring(g.id)
        genre_map[clean(g.slug):lower()] = tostring(g.id)
      end
    end
  end
  local out = {}
  for _, tag in ipairs(tags) do
    local id = genre_map[clean(tag):lower()] or tostring(tag)
    if id ~= "" then out[#out + 1] = id end
  end
  return out
end

methods.search = function(params)
  local q = clean(params.title or params.query or params.q)
  local page = math.max(1, tonumber(params.page) or 1)
  local path = "/comics?page=" .. page .. "&size=20&sort=newest"
  if q ~= "" then path = path .. "&name=" .. host.url_encode(q) end
  local ids = genre_ids(params.tags or params.genres)
  for i, id in ipairs(ids) do path = path .. "&genres%5B" .. (i - 1) .. "%5D=" .. host.url_encode(id) end
  local data, err = get_json(path)
  if not data then return { ok = false, error = err } end

  local items = {}
  for _, it in ipairs(data.list or {}) do
    if it.id then
      items[#items + 1] = {
        id = tostring(it.id),
        name = clean(it.name),
        cover = it.image or "",
        description = clean(it.description),
        url = SITE .. "/comic/" .. tostring(it.slug or "") .. "-pid" .. tostring(it.id),
      }
    end
  end
  return { ok = true, data = {
    items = items,
    total = tonumber(data.totalCount) or #items,
    pages = tonumber(data.totalPage) or 1,
  } }
end

methods.detail = function(params)
  local id = comic_id(params.id)
  if id == "" then return { ok = true, data = {} } end
  local d, err = get_json("/comics/" .. host.url_encode(id) .. "/view")
  if not d then return { ok = false, error = err } end
  return { ok = true, data = {
    id = tostring(d.id or id),
    name = clean(d.name),
    cover = d.image or "",
    description = clean(d.description),
    status = host.text.normalize_status(clean(d.status)),
    tags = (type(d.genre_names) == "string" and split_names(d.genre_names)) or names(d.genres),
    genres = names(d.genres),
    release_at = epoch(d.published_date),
    url = SITE .. "/comic/" .. tostring(d.slug or "") .. "-pid" .. tostring(d.id or id),
  } }
end

methods.chapters = function(params)
  local id = comic_id(params.id)
  if id == "" then return { ok = true, data = {} } end
  local d, err = get_json("/comics/" .. host.url_encode(id) .. "/view")
  if not d then return { ok = false, error = err } end
  local out = {}
  for _, c in ipairs(d.news_chapters or {}) do
    if c.id then
      out[#out + 1] = {
        id = id .. ":" .. tostring(c.id),
        number = tonumber(c.chapter_number) or 0,
        title = clean(c.name),
        release_at = epoch(c.published_date),
        pages = 0,
        url = SITE .. "/comic/" .. tostring(d.slug or "") .. "-pid" .. id .. "/" .. tostring(c.slug or ""),
      }
    end
  end
  table.sort(out, function(a, b) return a.number < b.number end)
  return { ok = true, data = out }
end

methods.pages = function(params)
  local composite = tostring(params.id or "")
  local _, chapter = composite:match("^([^:]+):(.+)$")
  if not chapter then return { ok = true, data = {} } end
  local d, err = get_json("/chapters/" .. host.url_encode(chapter) .. "/view")
  if not d then return { ok = false, error = err } end
  local out = {}
  for _, img in ipairs(d.chapter_images or {}) do
    local url = type(img) == "table" and img.image or img
    if type(url) == "string" and url ~= "" then out[#out + 1] = url end
  end
  return { ok = true, data = out }
end

methods.genres = function()
  local data, err = get_json("/genres?size=500")
  if not data then return { ok = false, error = err } end
  return { ok = true, data = names(data.genres) }
end

function handle(req)
  local fn = methods[req.method]
  if not fn then return { ok = false, error = "mangade: unknown method " .. tostring(req.method) } end
  local ok, res = pcall(fn, req.params or {})
  if not ok then return { ok = false, error = "mangade: " .. tostring(res) } end
  return res
end
