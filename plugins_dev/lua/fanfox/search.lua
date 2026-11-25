local c = require("common")
local DESKTOP = c.DESKTOP
local MOBILE = c.MOBILE
local util = c.util
local genres = c.genres
local name_to_slug = c.name_to_slug
local manga_html = c.manga_html
return function(params)
  local query = tostring(params.title or params.query or params.q or "")
  local genres_filter = type(params.genres) == "table" and params.genres
    or type(params.tags) == "table" and params.tags
    or {}
  if query == "" and #genres_filter == 0 then
    return { ok = false, error = "fanfox: empty query" }
  end
  local page = tonumber(params.page) or 1
  local url = DESKTOP .. "/search?title=" .. host.url_encode(query) .. "&page=" .. tostring(page)

  local ids = {}
  for _, g in ipairs(genres_filter) do
    if type(g) == "table" then
      g = g.slug or g.name
    end
    g = tostring(name_to_slug[string.lower(tostring(g))] or g)
    if g ~= "" then
      ids[#ids + 1] = host.url_encode(g)
    end
  end
  if #ids > 0 then
    url = url .. "&genres=" .. table.concat(ids, ",")
  end

  local html = util.http_body(url)
  if not html then
    return { ok = false, error = "fanfox: search failed" }
  end
  local items = util.parse_search(html)
  return { ok = true, data = { items = items, total = #items } }
end
