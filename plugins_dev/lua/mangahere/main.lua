-- MangaHere (www.mangahere.cc): server-rendered search/detail; chapter pages
-- hide behind a Dean-Edwards packer served by chapterfun.ashx (2 images per
-- call, empty key ok). Port of goIsekai examples/plugins/lua/mangahere;
-- host.re2.* replaces host.regex.* (pattern first), envelope
-- { ok, data | error }, canonical fields per the mangadex plugin.
local BASE = "https://www.mangahere.cc" -- manifest site_url; no params, pinned like the other house ports

local util = require("util")
local genres = require("genres")

-- ids arrive as bare strings; tolerate {id=...} wrappers like the source did
local function id_of(params)
  local v = params.id
  if type(v) == "table" then v = v.id end
  return tostring(v or "")
end

-- request -> (body, err): the host two-value fetch_body form; opts (with
-- opts.headers) pass straight through.
local http_body = host.http.fetch_body

local methods = {}

-- ── search ─────────────────────────────────────────────────────────────────
-- /search?title=Q&page=N; genre browse adds &genres=<numeric id> — the empty
-- title is valid and returns genre-only cards (HAR-confirmed upstream).

methods.search = function(params)
  local q = tostring(params.title or params.query or params.q or "")
  local page = math.max(1, tonumber(params.page) or 1)
  local tags = params.tags or params.genres or {}
  if type(tags) == "string" then -- UI joins checked genres with commas
    local list = {}
    for name in tags:gmatch("[^,]+") do list[#list + 1] = name:match("^%s*(.-)%s*$") end
    tags = list
  end

  local url
  if type(tags) == "table" and #tags > 0 then
    local gid = tostring(tags[1] or "")
    gid = genres.map[gid] or gid
    url = BASE .. "/search?title=" .. host.url_encode(q) .. "&genres=" .. host.url_encode(gid) .. "&nogenres=&sort=name"
    if page > 1 then url = url .. "&page=" .. tostring(page) end
  elseif q ~= "" then
    url = BASE .. "/search?title=" .. host.url_encode(q) .. "&page=" .. tostring(page)
  else
    return { ok = true, data = { items = {}, total = 0 } }
  end

  local html = http_body(url)
  if not html or html == "" then return { ok = false, error = "mangahere: search failed" } end
  local items = util.parse_search(html)
  return { ok = true, data = { items = items, total = #items } }
end

-- ── detail / chapters ──────────────────────────────────────────────────────
-- Both parse /manga/{id}/; chapter rows carry cXX slugs + "Nov 18,2016" dates.

methods.detail = function(params)
  local id = id_of(params)
  if id == "" then return { ok = true, data = {} } end
  local html = http_body(BASE .. "/manga/" .. id .. "/")
  if not html or html == "" then return { ok = false, error = "mangahere: detail fetch failed" } end
  return { ok = true, data = util.parse_manga_detail(html, id) }
end

methods.chapters = function(params)
  local id = id_of(params)
  if id == "" then return { ok = true, data = {} } end
  local html = http_body(BASE .. "/manga/" .. id .. "/")
  if not html or html == "" then return { ok = false, error = "mangahere: chapter list fetch failed" } end
  return { ok = true, data = util.parse_chapter_list(html, id) }
end

-- ── pages ──────────────────────────────────────────────────────────────────
-- Chapter id "slug:c007.5" -> path "slug/c007.5"; util.get_page_urls resolves
-- cid from the reader page and walks chapterfun.ashx in pairs.

methods.pages = function(params)
  local chapter_id = id_of(params)
  if chapter_id == "" then return { ok = true, data = {} } end
  local urls = util.get_page_urls(http_body, chapter_id:gsub(":", "/"))
  if not urls then return { ok = true, data = {} } end
  return { ok = true, data = urls }
end

-- ── genres ─────────────────────────────────────────────────────────────────

methods.genres = function()
  return { ok = true, data = genres.names }
end

function handle(req)
  local fn = methods[req.method]
  if fn == nil then return { ok = false, error = "unknown method " .. tostring(req.method) } end
  return fn(req.params or {})
end
