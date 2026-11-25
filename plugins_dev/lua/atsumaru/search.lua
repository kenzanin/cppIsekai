local c = require("common")
local BASE = c.BASE
local CDN = c.CDN
local HEADERS = c.HEADERS
local GENRE_SLUGS = c.GENRE_SLUGS
local normalize_status = c.normalize_status
local image_url = c.image_url
local get_json = c.get_json
local epoch_ms = c.epoch_ms
local strip_html = c.strip_html
return function(params)
  local q = params.title or params.query or params.q or ""
  local page = math.max(1, tonumber(params.page) or 1)
  local tags = params.tags or {}
  if type(tags) == "string" then
    local list = {}
    for name in tags:gmatch("[^,]+") do list[#list + 1] = name end
    tags = list
  end
  local filter = ""
  if type(tags) == "table" and #tags > 0 then
    local slugs = {}
    for _, name in ipairs(tags) do
      local slug = GENRE_SLUGS[name]
      if slug then slugs[#slugs + 1] = slug end
    end
    if #slugs > 0 then
      filter = " && genreIds:=[" .. table.concat(slugs, ",") .. "]"
    end
  end
  local url = BASE .. "/collections/manga/documents/search?q="
    .. host.url_encode(q ~= "" and q or "*")
    .. "&filter_by=" .. host.url_encode(filter)
    .. "&query_by=" .. host.url_encode("title,englishTitle,otherNames,authors")
    .. "&page=" .. tostring(page)
    .. "&per_page=20"
  local data, err = get_json(url)
  if err ~= nil then return { ok = false, error = err } end
  local items = {}
  if type(data.hits) == "table" then
    for _, hit in ipairs(data.hits) do
      local doc = hit.document
      if type(doc) == "table" and doc.id then
        items[#items + 1] = {
          id = doc.id,
          name = doc.title or doc.englishTitle or "",
          cover = image_url(doc.poster or doc.posterSmall),
        }
      end
    end
  end
  return { ok = true, data = { items = items, total = tonumber(data.found) or #items } }
end
