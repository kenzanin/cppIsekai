-- comizy.io: Next.js manga site, API-first (api.comizy.io + /_next/data
-- buildId routes). Port of goIsekai examples/plugins/lua/comizy to the
-- cppIsekai host ABI (host.http.fetch + status check, json globals).
-- Pages are plain URL strings; Referer comes from the host image-proxy
-- fallback to site_url, same as the source relied on.
local API = "https://api.comizy.io" -- single-domain pair; main.json params kept verbatim, unused
local WEB = "https://comizy.io"

-- GET + 200 + JSON decode -> table, or nil (goIsekai get_json semantics).
local function get_json(url)
  local res, err = host.http.fetch(url, { headers = { accept = "application/json" } })
  if err ~= nil or res.status ~= 200 then return nil end
  local ok, decoded = pcall(json.decode, res.body or "")
  if not ok or type(decoded) ~= "table" then return nil end
  return decoded
end

-- buildId rotates per deploy; cache per VM lifetime. genres cache + slug map
-- live at file scope so every method shares them.
local cached_build_id = nil
local cached_genres, name_to_slug = nil, {}
local function build_id()
  if cached_build_id then return cached_build_id end
  local v = get_json(WEB .. "/api/version")
  if v and v.buildId then cached_build_id = v.buildId end
  return cached_build_id
end

local function split_id(id)
  id = tostring(id or "")
  local sep = id:find(".", 1, true)
  if not sep then return nil, nil end
  return id:sub(1, sep - 1), id:sub(sep + 1)
end

-- comizy labels a split chapter "Chapter 12-2" and a decimal one "Chapter
-- 12.1"; chapter_num reads the leading number only, so the hyphen form
-- collapsed to 12 and collided with its sibling. Fold hyphen -> dot.
local function comizy_chapter_num(title)
  local t = (tostring(title or ""):gsub("^(%s*[Cc]hapter%s+%d+)%s*%-%s*(%d+)%s*$", "%1.%2"))
  return host.text.chapter_num(t)
end

local methods = {}

-- [{id,slug,name,cover,...}] -> canonical search items, id packed.
local function items_from(raw)
  local items = {}
  for _, it in ipairs(raw) do
    if it.id and it.slug then
      items[#items + 1] = {
        id = tostring(it.id) .. "." .. tostring(it.slug),
        name = it.name or it.slug,
        cover = it.cover or "",
      }
    end
  end
  return { ok = true, data = { items = items, total = #items } }
end

-- ── search ─────────────────────────────────────────────────────────────────
-- id packs "{mangaId}.{slug}" (split at first dot; both halves have no dots): chapters need the opaque mangaId, pages need
-- the slug — neither derives the other. Genre mode browses the SSR JSON
-- (the REST search silently ignores genre params); query is ignored there.
methods.search = function(params)
  local query = tostring(params.title or params.query or params.q or "")
  local page = tonumber(params.page) or 1
  local genres = type(params.tags) == "table" and params.tags
    or type(params.genres) == "table" and params.genres or {}

  if #genres > 0 then
    local bid = build_id()
    if not bid then
      return { ok = false, error = "comizy: version backend unreachable" }
    end
    local g = genres[1]
    if type(g) == "table" then g = g.slug or g.name end
    -- hosts echo the genres() name back; the browse route wants the slug
    local gslug = tostring(name_to_slug[string.lower(tostring(g))] or g)
    local url = WEB .. "/_next/data/" .. bid .. "/genres/"
      .. host.url_encode(gslug) .. ".json?slug=" .. host.url_encode(gslug)
    if page > 1 then
      url = url .. "&page=" .. tostring(page)
    end
    local res = get_json(url)
    if not res then
      return { ok = false, error = "comizy: genre browse failed" }
    end
    return items_from(((res.pageProps or {}).items) or {})
  end

  if query == "" then
    return { ok = true, data = { items = {}, total = 0 } }
  end

  local url = API .. "/titles/search?q=" .. host.url_encode(query)
    .. "&page=" .. tostring(page) .. "&limit=24"
  local res = get_json(url)
  if not res then
    return { ok = false, error = "comizy: search failed" }
  end
  return items_from(((res.data or {}).items) or {})
end

-- ── genres ─────────────────────────────────────────────────────────────────
-- GET /genres (~71 entries). Names per house convention; the slug map feeds
-- genre browse (the /genres/<slug> route keys on slug, not name).
methods.genres = function()
  if cached_genres then return { ok = true, data = cached_genres } end
  local names = {}
  local res = get_json(API .. "/genres")
  if res then
    for _, g in ipairs(((res.data or {}).items) or {}) do
      if g.slug and g.name then
        names[#names + 1] = tostring(g.name)
        name_to_slug[string.lower(tostring(g.name))] = tostring(g.slug)
      end
    end
    table.sort(names)
  end
  cached_genres = names
  return { ok = true, data = names }
end

-- ── detail ─────────────────────────────────────────────────────────────────
methods.detail = function(params)
  local manga_id = tostring(params.id or "")
  local _, slug = split_id(manga_id)
  if not slug then
    return { ok = true, data = { id = manga_id } }
  end

  local bid = build_id()
  if not bid then
    return { ok = false, error = "comizy: version backend unreachable" }
  end
  local res = get_json(WEB .. "/_next/data/" .. bid .. "/" .. slug
    .. ".json?slug=" .. host.url_encode(slug))
  local m = res and (res.pageProps or {}).initialManga
  if not m then
    return { ok = true, data = { id = manga_id } }
  end

  local genres = {}
  for _, g in ipairs(m.genres or {}) do
    genres[#genres + 1] = g.name or g.slug
  end
  local authors = {}
  if m.authors and m.authors[1] and m.authors[1].name then
    authors[#authors + 1] = m.authors[1].name
  end

  return { ok = true, data = {
    id = manga_id,
    name = m.name or "",
    cover = m.cover or "",
    status = host.text.normalize_status(m.status or ""),
    tags = genres,
    -- no machine date on the SSR payload
    release_at = 0,
    description = m.summary or "",
    authors = authors,
    artists = {},
    url = WEB .. "/" .. slug,
  } }
end

-- ── chapters ───────────────────────────────────────────────────────────────
-- Dedicated endpoint; already newest-first, which is the house order (the
-- host sorts by number and reader prev/next assumes newest-first). Do NOT
-- reverse. Chapter id packs "{mangaSlug}.{chapterSlug}".
methods.chapters = function(params)
  local manga_id = tostring(params.id or "")
  local mid, slug = split_id(manga_id)
  if not mid then
    return { ok = true, data = {} }
  end

  local res = get_json(API .. "/titles/" .. host.url_encode(mid) .. "/chapters")
  if not res then
    return { ok = false, error = "comizy: chapters failed" }
  end
  local out = {}
  for _, c in ipairs(((res.data or {}).chapters) or {}) do
    if c.slug then
      local title = c.name or c.slug
      out[#out + 1] = {
        id = slug .. "." .. c.slug,
        number = comizy_chapter_num(title) or 0,
        title = title,
        release_at = c.updated_at or 0, -- ISO string from the API, pass-through
        pages = 0,
      }
    end
  end
  return { ok = true, data = out }
end

-- ── pages ──────────────────────────────────────────────────────────────────
methods.pages = function(params)
  local chapter_id = tostring(params.id or "")
  local mslug, cslug = split_id(chapter_id)
  if not mslug then
    return { ok = false, error = "comizy: bad chapter id " .. chapter_id }
  end

  local bid = build_id()
  if not bid then
    return { ok = false, error = "comizy: version backend unreachable" }
  end
  local res = get_json(WEB .. "/_next/data/" .. bid .. "/" .. mslug .. "/"
    .. cslug .. ".json?slug=" .. host.url_encode(mslug)
    .. "&chapter-slug=" .. host.url_encode(cslug))
  local ch = res and (res.pageProps or {}).initialChapter
  local imgs = ch and ch.images or {}
  if #imgs == 0 then
    return { ok = false, error = "comizy: no pages for " .. chapter_id }
  end
  return { ok = true, data = imgs }
end

function handle(req)
  local fn = methods[req.method]
  if fn == nil then return { ok = false, error = "unknown method " .. tostring(req.method) } end
  return fn(req.params or {})
end
