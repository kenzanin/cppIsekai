local c = require("common")
local SITE = c.SITE
local API = c.API
local UA = c.UA
local HEADERS = c.HEADERS
local trim = c.trim
local abs_url = c.abs_url
local abs_url = c.abs_url
local tags_to_names = c.tags_to_names
local decode_id = c.decode_id
local http_get = c.http_get
local http_post_json = c.http_post_json
return function(params)
  local query = tostring(params.title or params.query or params.q or "")
  local genres = type(params.tags) == "table" and params.tags or type(params.genres) == "table" and params.genres or {}
  local page = math.max(1, tonumber(params.page) or 1)
  local limit = math.max(1, tonumber(params.limit) or 12)
  local want = page * limit

  local base = API .. "/search?source=comick&sort=relevance&limit=100" .. "&contentRating=safe&contentRating=suggestive"
  if query ~= "" then
    base = base .. "&q=" .. host.url_encode(query)
  end
  if #genres > 0 then
    local g = genres[1]
    if type(g) == "table" then
      g = g.slug or g.name
    end
    base = base .. "&comickTags=" .. host.url_encode(tostring(g))
  end

  local items = {}
  local offset = 0
  local total = 0
  for _ = 1, 20 do
    local data = http_get(base .. "&offset=" .. tostring(offset))
    if not data or type(data.items) ~= "table" or #data.items == 0 then
      break
    end
    total = tonumber(data.total) or total
    for _, it in ipairs(data.items) do
      items[#items + 1] = {
        id = tostring(it.id or ""),
        name = trim(it.title or ""),
        cover = abs_url(it.coverUrl),
      }
    end
    offset = offset + #data.items
    if (total > 0 and offset >= total) or offset >= want then
      break
    end
  end
  if total <= 0 then
    total = #items
  end
  -- The host paginates the returned slice; the plugin only fetches enough
  -- upstream rows to cover the requested page.
  local first = (page - 1) * limit + 1
  local last = math.min(#items, page * limit)
  local sliced = {}
  for i = first, last do
    sliced[#sliced + 1] = items[i]
  end
  return { ok = true, data = { items = sliced, total = total, pages = math.ceil(total / limit) } }
end
