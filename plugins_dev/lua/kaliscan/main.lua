-- KaliScan (kaliscan.me): search results come from a backend HTML fragment,
-- chapters from the detail page, page images from a chapterServer fragment.
-- Port of goIsekai examples/plugins/lua/kaliscan; host.re2.* replaces
-- host.regex.* (pattern-first args), host.http.fetch replaces get_body.
-- Envelope { ok, data | error }; canonical manga fields per the mangadex plugin.
local BASE = "https://kaliscan.me" -- manifest site_url; source notes flag the domain as dead, kept for parity

local genres = require("genres")
local util = require("util")

-- One search walks at most this many backend fragment pages: each is a
-- separate request, and out-of-range pages wrap to page 1, so the walk below
-- stops as soon as a page adds nothing new to the deduped set.
local MAX_RESULT_PAGES = 5

-- http request -> (body, err). 200-or-nothing, the anisascans http_req shape.
local function http_req(url, opts)
  local res, err = host.http.fetch(url, opts or {})
  if err ~= nil then return nil, err end
  if res.status ~= 200 then return nil, "kaliscan: upstream status " .. tostring(res.status) end
  return res.body, nil
end

local methods = {}

-- ── search ─────────────────────────────────────────────────────────────────

methods.search = function(params)
  local q = tostring(params.title or params.query or params.q or "")
  local page = math.max(1, tonumber(params.page) or 1)
  local tags = params.tags or params.genres or {}
  if type(tags) == "string" then -- UI joins checked genres with commas
    local list = {}
    for name in tags:gmatch("[^,]+") do list[#list + 1] = name:match("^%s*(.-)%s*$") end
    tags = list
  end

  -- Genre browse: /genres/<slug> reuses the search card markup (util.parse_search).
  if type(tags) == "table" and #tags > 0 then
    local slug = genres.map[tags[1]] or tags[1]
    local url = BASE .. "/genres/" .. host.url_encode(slug)
    if page > 1 then url = url .. "?page=" .. tostring(page) end
    local body, err = http_req(url)
    if err ~= nil then return { ok = false, error = err } end
    local items = util.parse_search(body)
    return { ok = true, data = { items = items, total = #items } }
  end

  -- /search is client-rendered; results come from the backend fragment
  -- (10 cards per page, no paginator).
  local all, seen = {}, {}
  for p = 1, MAX_RESULT_PAGES do
    local body, err =
      http_req(BASE .. "/service/backend/search/?q=" .. host.url_encode(q) .. "&page=" .. tostring(p))
    if err ~= nil then
      if p == 1 then return { ok = false, error = err } end
      break
    end
    local added = 0
    for _, r in ipairs(util.parse_search(body)) do
      if not seen[r.id] then
        seen[r.id] = true
        all[#all + 1] = r
        added = added + 1
      end
    end
    if added == 0 then break end
  end
  return { ok = true, data = { items = all, total = #all } }
end

-- ── detail / chapters ──────────────────────────────────────────────────────

methods.detail = function(params)
  local id = tostring(params.id or "")
  local body, err = http_req(BASE .. "/manga/" .. id)
  if err ~= nil then return { ok = false, error = err } end
  return { ok = true, data = util.parse_manga_detail(body, id, BASE) }
end

methods.chapters = function(params)
  local id = tostring(params.id or "")
  local body, err = http_req(BASE .. "/manga/" .. id)
  if err ~= nil then return { ok = false, error = err } end
  return { ok = true, data = util.parse_chapter_list(body, id) }
end

-- ── pages ──────────────────────────────────────────────────────────────────
-- Chapter id "slug:chapter-N" -> /manga/slug/chapter-N; that page carries
-- chapterId=N, and the images live on the chapterServer fragment. The request
-- needs the chapter Referer; returned pages are plain URL strings (per-page
-- image headers are a host-side concern, see the site-origin Referer fallback).

methods.pages = function(params)
  local path = tostring(params.id or ""):gsub(":", "/")
  local body, err = http_req(BASE .. "/manga/" .. path)
  if err ~= nil then return { ok = false, error = err } end
  local chapter_id = util.find1(body, [[chapterId\s*=\s*(\d+)]])
  if not chapter_id then return { ok = true, data = {} } end
  local img_body, ierr = http_req(BASE .. "/service/backend/chapterServer/?server_id=1&chapter_id=" .. chapter_id, {
    headers = { Referer = BASE .. "/manga/" .. path, ["X-Requested-With"] = "XMLHttpRequest" },
  })
  if ierr ~= nil then return { ok = false, error = ierr } end
  return { ok = true, data = util.parse_page_list(img_body) }
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
