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
