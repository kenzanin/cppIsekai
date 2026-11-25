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
  local id = params.id or ""
  local data, err = get_json(BASE .. "/api/manga/page?id=" .. host.url_encode(id))
  if err ~= nil then return { ok = false, error = err } end
  local page = type(data) == "table" and data.mangaPage
  if type(page) ~= "table" then
    return { ok = false, error = "atsumaru: no mangaPage for " .. tostring(id) }
  end
  local poster = page.poster
  if type(poster) == "table" then poster = poster.image or poster.id end
  local authors = {}
  for _, a in ipairs(page.authors or {}) do
    if type(a) == "table" and a.name then authors[#authors + 1] = a.name end
  end
  local genres = {}
  for _, g in ipairs(page.genres or {}) do
    if type(g) == "table" and g.name then genres[#genres + 1] = g.name end
  end
  -- Discriminator is `medium` ("Comic" vs "Novel"), NOT `hasUploads`:
  -- the latter is false even on manga with real uploaded pages (e.g. 5QVaH, 31 pages).
  -- `type` is a format label (Manga/Manwha) and does not identify novels either.
  local medium = tostring(page.medium or ""):lower()
  local is_novel = medium:find("novel", 1, true) ~= nil
    or tostring(page.type or ""):lower():find("novel", 1, true) ~= nil
  return {
    ok = true,
    data = {
      id = id,
      name = page.title or "",
      cover = image_url(poster),
      status = normalize_status(page.status),
      tags = genres,
      release_at = 0,
      description = strip_html(page.synopsis),
      authors = authors,
      artists = {},
      url = BASE .. "/manga/" .. tostring(id),
      is_novel = is_novel,
    },
  }
end
