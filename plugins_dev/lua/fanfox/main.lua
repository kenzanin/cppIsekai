-- fanfox.net / m.fanfox.net (DM5 stack). Port of goIsekai
-- examples/plugins/lua/fanfox to the cppIsekai host ABI (host.http.fetch +
-- status check; all parsing and the chapterfun page walk live in util.lua).
-- Pages are plain URL strings: the per-image Referer is folded into the
-- chapterfun request and image fetches rely on the host image-proxy fallback.
local DESKTOP = "https://fanfox.net" -- search + chapterfun; mobile has no search endpoint
local MOBILE = "https://m.fanfox.net" -- detail/chapters pages

local util = require("util")
local genres = require("genres")

-- genres() objects carry the slug; a bare-name caller needs name -> id.
local name_to_slug = {}
for _, g in ipairs(genres.list) do
  name_to_slug[string.lower(g.name)] = g.slug
end

local methods = {}

-- ── search ─────────────────────────────────────────────────────────────────
-- Desktop site only. Genre filter takes comma-separated numeric ids; the
-- genre browse passes an empty title, so an empty query is only rejected
-- when no genre was asked for either.
methods.search = function(params)
  local query = tostring(params.title or params.query or params.q or "")
  local genres_filter = type(params.genres) == "table" and params.genres
    or type(params.tags) == "table" and params.tags or {}
  if query == "" and #genres_filter == 0 then
    return { ok = false, error = "fanfox: empty query" }
  end
  local page = tonumber(params.page) or 1
  local url = DESKTOP .. "/search?title=" .. host.url_encode(query)
    .. "&page=" .. tostring(page)

  local ids = {}
  for _, g in ipairs(genres_filter) do
    if type(g) == "table" then g = g.slug or g.name end
    g = tostring(name_to_slug[string.lower(tostring(g))] or g)
    if g ~= "" then ids[#ids + 1] = host.url_encode(g) end
  end
  if #ids > 0 then url = url .. "&genres=" .. table.concat(ids, ",") end

  local html = util.http_body(url)
  if not html then
    return { ok = false, error = "fanfox: search failed" }
  end
  local items = util.parse_search(html)
  return { ok = true, data = { items = items, total = #items } }
end

-- ── detail / chapters ──────────────────────────────────────────────────────
-- Both parse the mobile detail page: name, cover, status, genres, summary
-- and the full chapter list live there.
local function manga_html(manga_id)
  if manga_id == "" then return nil end
  return util.http_body(MOBILE .. "/manga/" .. manga_id .. "/")
end

methods.detail = function(params)
  local manga_id = tostring(params.id or "")
  local html = manga_html(manga_id)
  if not html then
    return { ok = false, error = "fanfox: detail failed" }
  end
  return { ok = true, data = util.parse_manga_detail(html, manga_id) }
end

methods.chapters = function(params)
  local manga_id = tostring(params.id or "")
  local html = manga_html(manga_id)
  if not html then
    return { ok = false, error = "fanfox: chapters failed" }
  end
  return { ok = true, data = util.parse_chapter_list(html, manga_id) }
end

-- ── pages ──────────────────────────────────────────────────────────────────
-- Chapter ids marshal with ":" between slug and chapter; the URL path
-- wants "/" ("blue_lock:c161" -> "blue_lock/c161").
methods.pages = function(params)
  local chapter_id = (tostring(params.id or ""):gsub(":", "/"))
  if chapter_id == "" then
    return { ok = false, error = "fanfox: bad chapter id" }
  end
  local urls = util.get_page_urls(chapter_id)
  if urls == nil or #urls == 0 then
    return { ok = false, error = "fanfox: no pages for " .. chapter_id }
  end
  return { ok = true, data = urls }
end

-- ── genres ─────────────────────────────────────────────────────────────────
-- Static list; objects, not bare names — the numeric slug is the payload
-- search?genres= consumes (batcave precedent).
methods.genres = function()
  return { ok = true, data = genres.list }
end

function handle(req)
  local fn = methods[req.method]
  if fn == nil then return { ok = false, error = "unknown method " .. tostring(req.method) } end
  return fn(req.params or {})
end
