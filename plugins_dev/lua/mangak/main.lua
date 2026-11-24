-- MangaK (mangak.io): Next.js front end over a public JSON API at
-- api.mangak.io. Ported from goIsekai's wasm plugin plus tmp/mangak.har;
-- the HAR confirmed the live shapes (pagination, genres, chapter images).
-- Every response is {"success":true,"data":...}; no HTML scraping, no crypto.
local API = host.plugin.param("api") or "https://api.mangak.io"
local SITE = host.plugin.param("site_url") or "https://mangak.io"
local OPTS = {
  timeout_ms = 45000,
  accept = "application/json, text/plain, */*",
  headers = { Referer = SITE .. "/" },
}

local function get_json(path)
  local body, err = host.http.fetch_body(API .. path, OPTS)
  if not body or body == "" then return nil, err end
  local ok, doc = pcall(json.decode, body)
  if not ok or type(doc) ~= "table" or doc.success ~= true then
    return nil, "mangak: bad envelope"
  end
  return doc.data
end

local function find1(s, pat)
  return host.re2.capture(pat, tostring(s or ""))
end

local function names(list)
  local out = {}
  if type(list) ~= "table" then return out end
  for _, v in ipairs(list) do
    local name = type(v) == "table" and (v.name or v.title) or v
    if type(name) == "string" and name ~= "" then out[#out + 1] = host.text.unescape(name) end
  end
  return out
end

local function epoch(v)
  return host.text.date_epoch(tostring(v or "")) or 0
end

local genre_map
local function ensure_genres()
  if genre_map then return genre_map end
  genre_map = {}
  local data = get_json("/genres")
  if data and type(data.items) == "table" then
    for _, g in ipairs(data.items) do
      if type(g) == "table" and g.name then
        genre_map[host.text.trim(g.name)] = g.slug or g.name
        genre_map[tostring(g.name):lower()] = g.slug or g.name
      end
    end
  end
  return genre_map
end

local function slug_of(id)
  id = tostring(id or "")
  return find1(id, [[/([^/?#]+)]]) or id
end

methods = {}

methods.search = function(params)
  local q = host.text.trim(tostring(params.title or params.query or params.q or ""))
  local page = math.max(1, tonumber(params.page) or 1)
  local limit = math.max(1, math.min(60, tonumber(params.limit) or 24))
  local tags = params.tags or params.genres or {}
  if type(tags) == "string" then
    local list = {}
    for name in tags:gmatch("[^,]+") do list[#list + 1] = host.text.trim(name) end
    tags = list
  end
  local path = "/titles/search?q=" .. host.url_encode(q) .. "&limit=" .. limit .. "&page=" .. page
  if type(tags) == "table" and #tags > 0 then
    local slug = ensure_genres()[tags[1]] or tags[1]
    path = path .. "&genres=" .. host.url_encode(slug)
  end
  local data, err = get_json(path)
  if not data then return { ok = false, error = err } end

  local items = {}
  for _, it in ipairs(data.items or {}) do
    if it.id and it.id ~= "" then
      items[#items + 1] = {
        id = it.id,
        name = host.text.unescape(it.name or ""),
        cover = it.cover or "",
        status = host.text.normalize_status(it.status or ""),
        url = SITE .. (it.url or ("/manga/" .. it.id)),
      }
    end
  end
  local pg = data.pagination or {}
  return { ok = true, data = { items = items, total = tonumber(pg.total) or #items,
                               pages = tonumber(pg.total_pages) or 1 } }
end

methods.detail = function(params)
  local id = slug_of(params.id or "")
  if id == "" then return { ok = true, data = {} } end
  local data, err = get_json("/titles/" .. host.url_encode(id))
  if not data then return { ok = false, error = err } end
  local t = data.title or {}
  return { ok = true, data = {
    id = t.id or id,
    name = host.text.unescape(t.name or ""),
    cover = t.cover or "",
    description = host.text.unescape(t.summary or ""),
    status = host.text.normalize_status(t.status or ""),
    tags = names(t.genres),
    authors = names(t.authors),
    artists = names(t.artists),
    release_at = epoch(t.release_date),
    url = SITE .. (t.url or ("/manga/" .. id)),
  } }
end

methods.chapters = function(params)
  local id = slug_of(params.id or "")
  if id == "" then return { ok = true, data = {} } end
  local data, err = get_json("/titles/" .. host.url_encode(id) .. "/chapters")
  if not data then return { ok = false, error = err } end
  local out = {}
  for _, c in ipairs(data.chapters or {}) do
    if c.id and c.id ~= "" then
      out[#out + 1] = {
        id = id .. ":" .. c.id,
        number = tonumber(c.number) or 0,
        title = host.text.unescape(c.name or "Chapter"),
        release_at = epoch(c.updated_at),
        pages = 0,
        url = SITE .. (c.url or ""),
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
  local data, err = get_json("/titles/" .. host.url_encode(id) .. "/chapters/" .. host.url_encode(chapter))
  if not data then return { ok = false, error = err } end
  local c = data.chapter or {}
  local out = {}
  for _, url in ipairs(c.images or {}) do
    if type(url) == "string" and url ~= "" then out[#out + 1] = url end
  end
  return { ok = true, data = out }
end

methods.genres = function()
  local data = get_json("/genres")
  return { ok = true, data = data and names(data.items) or {} }
end

function handle(req)
  local fn = methods[req.method]
  if not fn then return { ok = false, error = "mangak: unknown method " .. tostring(req.method) } end
  local ok, res = pcall(fn, req.params or {})
  if not ok then return { ok = false, error = "mangak: " .. tostring(res) } end
  return res
end
