local c = require("common")
local API = c.API
local WEB = c.WEB
local get_json = c.get_json
local cached_build_id = c.cached_build_id
local cached_genres = c.cached_genres
local name_to_slug = c.name_to_slug
local build_id = c.build_id
local split_id = c.split_id
local comizy_chapter_num = c.comizy_chapter_num
local items_from = c.items_from
return function(params)
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
