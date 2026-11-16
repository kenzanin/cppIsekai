-- comico.moe: comick-backed aggregator; catalogue mirrors comick, the reader
-- proxies every source. Port of goIsekai examples/plugins/lua/comico to the
-- cppIsekai host ABI (host.http.fetch + status check, json globals).
-- Pages are plain URL strings; the per-page Referer header is dropped (the
-- host image-proxy falls back to site_url, same as comizy).
local SITE = "https://comico.moe" -- single-domain aggregator
local API = SITE .. "/api"
local UA = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36"
local HEADERS = { ["User-Agent"] = UA, ["Accept"] = "application/json" }

-- host.text.trim casefolds (utf8proc CASEFOLD) — mangles titles; local trim.
local function trim(s)
  return (tostring(s or ""):gsub("^%s+", ""):gsub("%s+$", ""))
end

local function abs_url(u)
  if type(u) ~= "string" or u == "" then return "" end
  if u:sub(1, 4) == "http" then return u end
  return SITE .. u
end

-- [{id,name,group}] -> {"name", ...}
local function tags_to_names(tags)
  local out = {}
  for _, t in ipairs(tags or {}) do
    if type(t) == "table" and t.name then out[#out + 1] = tostring(t.name) end
  end
  return out
end

-- goIsekai took JSON-string args; cppIsekai params are tables (or bare ids).
local function decode_id(v)
  if type(v) == "table" then v = v.id end
  return v
end

-- GET + 200 + JSON decode -> table, or nil on any failure (goIsekai http_get).
local function http_get(url)
  local res, err = host.http.fetch(url, { headers = HEADERS })
  if err ~= nil or res.status ~= 200 then return nil end
  local ok, data = pcall(json.decode, res.body or "")
  if not ok or type(data) ~= "table" then return nil end
  return data
end

local function http_post_json(url, body)
  local res, err = host.http.fetch(url, {
    method = "POST",
    body = body,
    headers = {
      ["User-Agent"] = UA,
      ["Accept"] = "application/json",
      ["Content-Type"] = "application/json",
      ["Referer"] = SITE .. "/",
    },
  })
  if err ~= nil or res.status ~= 200 then return nil end
  return res.body
end

local methods = {}

-- ── search ─────────────────────────────────────────────────────────────────
-- paginates:false in main.json => every match; API 400s above limit=100, so
-- walk 100-row slices (source parity). Only the first genre maps to the
-- single comickTags filter the API takes.
methods.search = function(params)
  local query = tostring(params.title or params.query or params.q or "")
  local genres = type(params.tags) == "table" and params.tags
    or type(params.genres) == "table" and params.genres or {}

  local base = API .. "/search?source=comick&sort=relevance&limit=100"
    .. "&contentRating=safe&contentRating=suggestive"
  if query ~= "" then
    base = base .. "&q=" .. host.url_encode(query)
  end
  if #genres > 0 then
    local g = genres[1]
    if type(g) == "table" then g = g.slug or g.name end
    base = base .. "&comickTags=" .. host.url_encode(tostring(g))
  end

  local items = {}
  local offset = 0
  for _ = 1, 20 do
    local data = http_get(base .. "&offset=" .. tostring(offset))
    if not data or type(data.items) ~= "table" or #data.items == 0 then break end
    for _, it in ipairs(data.items) do
      items[#items + 1] = {
        id = tostring(it.id or ""),
        name = trim(it.title or ""),
        cover = abs_url(it.coverUrl),
      }
    end
    offset = offset + #data.items
    if offset >= (tonumber(data.total) or 0) then break end
  end
  return { ok = true, data = { items = items, total = #items } }
end

-- ── detail ─────────────────────────────────────────────────────────────────
methods.detail = function(params)
  local manga_id = tostring(decode_id(params.id) or "")
  if manga_id == "" then
    return { ok = false, error = "comico: missing id" }
  end
  local data = http_get(API .. "/manga/" .. host.url_encode(manga_id))
  if not data then
    -- keep the id so the host can retry, as in goIsekai
    return { ok = true, data = {
      id = manga_id, name = "", cover = "", status = "unknown", tags = {},
      release_at = 0, description = "", artists = {},
      authors = {}, url = SITE .. "/manga/" .. manga_id,
    } }
  end

  local author
  for _, a in ipairs(data.authors or {}) do
    if type(a) == "table" and a.name then author = a.name break
    elseif type(a) == "string" then author = a break end
  end

  return { ok = true, data = {
    id = tostring(data.id or manga_id),
    name = trim(data.title or ""),
    cover = abs_url(data.coverUrl),
    status = host.text.normalize_status(data.status or ""),
    tags = tags_to_names(data.tags),
    -- no machine date on the manga endpoint
    release_at = 0,
    description = data.description or "",
    authors = author and { author } or {},
    artists = {},
    url = SITE .. "/manga/" .. manga_id,
  } }
end

-- ── chapters ───────────────────────────────────────────────────────────────
-- Multi-source aggregator: walk sources x languages, 100 per call (the API
-- 400s on larger limits). Reader proxies every source, so all are readable.
local function chapter_pages(base, into)
  local offset = 0
  for _ = 1, 20 do
    local data = http_get(base .. "&offset=" .. tostring(offset))
    if not data or type(data.items) ~= "table" or #data.items == 0 then break end
    for _, ch in ipairs(data.items) do
      into[#into + 1] = {
        id = tostring(ch.id or ""),
        number = tonumber(ch.number) or 0,
        title = trim(ch.title or ""),
        release_at = ch.publishedAt or 0, -- API ISO string, pass-through
        pages = 0,
      }
    end
    offset = offset + #data.items
    if offset >= (tonumber(data.total) or 0) then break end
  end
end

methods.chapters = function(params)
  local manga_id = tostring(decode_id(params.id) or "")
  if manga_id == "" then
    return { ok = false, error = "comico: missing id" }
  end

  local sources = http_get(API .. "/manga/" .. host.url_encode(manga_id) .. "/sources")
  if type(sources) ~= "table" or #sources == 0 then
    -- fall back to the native catalogue rather than reporting no chapters
    sources = { { sourceId = "mangadex" } }
  end

  local chapters = {}
  for _, src in ipairs(sources) do
    local sid = type(src) == "table" and src.sourceId or nil
    local readable = type(src) == "table" and tonumber(src.readableChapterCount) or nil
    if sid and (readable == nil or readable > 0) then
      local langs = http_get(API .. "/manga/" .. host.url_encode(manga_id)
        .. "/languages?source=" .. host.url_encode(sid))
      if type(langs) ~= "table" then langs = {} end
      for _, lang in ipairs(langs) do
        if type(lang) == "string" then
          chapter_pages(API .. "/manga/" .. host.url_encode(manga_id)
            .. "/chapters?order=desc&sort=number&limit=100"
            .. "&source=" .. host.url_encode(sid)
            .. "&language=" .. host.url_encode(lang), chapters)
        end
      end
    end
  end

  -- newest first across every source and language (source parity)
  table.sort(chapters, function(a, b)
    return tostring(a.release_at or "") > tostring(b.release_at or "")
  end)
  return { ok = true, data = chapters }
end

-- ── pages ──────────────────────────────────────────────────────────────────
-- Two-step reader: access token, then POST /open; the reply carries relative
-- page paths with the session baked into the query string.
methods.pages = function(params)
  local chapter_id = tostring(decode_id(params.id) or "")
  if chapter_id == "" then
    return { ok = false, error = "comico: missing id" }
  end

  local access = http_get(API .. "/reader/access/" .. host.url_encode(chapter_id))
  if not access or not access.token then
    return { ok = false, error = "comico: no access token for " .. chapter_id }
  end

  local body = http_post_json(API .. "/reader/chapters/" .. host.url_encode(chapter_id)
    .. "/open?d=" .. host.url_encode(tostring(access.token)), '{"proxyMethod":"all"}')
  if not body then
    return { ok = false, error = "comico: reader open failed for " .. chapter_id }
  end

  local ok, opened = pcall(json.decode, body)
  if not ok or type(opened) ~= "table" then
    return { ok = false, error = "comico: bad reader payload for " .. chapter_id }
  end

  local out = {}
  for _, p in ipairs(opened.pages or {}) do
    out[#out + 1] = abs_url(p)
  end
  if #out == 0 then
    return { ok = false, error = "comico: no pages for " .. chapter_id }
  end
  return { ok = true, data = out }
end

-- ── genres ─────────────────────────────────────────────────────────────────
-- GET /api/tags?source=comick-metadata -> [{id,name,group}]. House convention
-- is the plain name list; the search filter passes the name through, as the
-- source did with its single genre value.
local cached_genres = nil
methods.genres = function()
  if cached_genres then return { ok = true, data = cached_genres } end
  local names = {}
  local data = http_get(API .. "/tags?source=comick-metadata")
  if type(data) == "table" then
    for _, t in ipairs(data) do
      if type(t) == "table" and t.name then names[#names + 1] = tostring(t.name) end
    end
    table.sort(names)
  end
  cached_genres = names
  return { ok = true, data = names }
end

function handle(req)
  local fn = methods[req.method]
  if fn == nil then return { ok = false, error = "unknown method " .. tostring(req.method) } end
  return fn(req.params or {})
end
