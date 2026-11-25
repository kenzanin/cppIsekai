local c = require("common")
local BASE = c.BASE
local helpers = c.helpers
local genres = c.genres
local find1 = c.find1
local bare_slug = c.bare_slug
local label_value = c.label_value
local epoch = c.epoch
return function(params)
  local q = tostring(params.title or params.query or params.q or "")
  local page = math.max(1, tonumber(params.page) or 1)
  local tags = params.tags or params.genres or {}
  if type(tags) == "string" then -- UI joins checked genres with commas
    local list = {}
    for name in tags:gmatch("[^,]+") do list[#list + 1] = name:match("^%s*(.-)%s*$") end
    tags = list
  end

  local items = {}
  if type(tags) == "table" and #tags > 0 then
    -- Genre browse renders HTML comic cards; first genre wins.
    local slug = tostring(tags[1] or "")
    slug = genres.map[slug] or slug
    local url = BASE .. "/series?genres=" .. host.url_encode(slug)
    if page > 1 then url = url .. "&page=" .. tostring(page) end
    local body = helpers.http_get(url)
    if not body then return { ok = false, error = "mangabuddy: genre browse failed" } end
    -- <a href="/series/{id}"> ... <img alt="Cover of {title}" data-src="{cover}">
    local rows = host.re2.find_all(
      [[(?s)<a[^>]*href="/series/([^/?"]+)"[^>]*>.*?<img[^>]*alt="Cover of ([^"]+)"[^>]*data-src="([^"]+)"]],
      body)
    for _, r in ipairs(rows) do
      if r[2] and r[2] ~= "" then
        items[#items + 1] = { id = r[2], name = host.text.unescape(r[3] or ""), cover = host.text.trim(r[4] or "") }
      end
    end
  else
    -- Text search: /api/search?search=Q&page=N -> {comics:[...]} (API paginates)
    local url = BASE .. "/api/search?search=" .. host.url_encode(q)
    if page > 1 then url = url .. "&page=" .. tostring(page) end
    local raw = helpers.http_get(url, { json = true })
    if not raw then return { ok = false, error = "mangabuddy: search failed" } end
    local ok, data = pcall(json.decode, raw)
    if ok and type(data) == "table" and type(data.comics) == "table" then
      for _, c in ipairs(data.comics) do
        if type(c.slug_hash) == "string" and c.slug_hash ~= "" then
          items[#items + 1] = { id = c.slug_hash, name = c.title or "", cover = c.image or "" }
        end
      end
    end
  end
  return { ok = true, data = { items = items, total = #items } }
end
