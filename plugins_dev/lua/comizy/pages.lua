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
  local chapter_id = tostring(params.id or "")
  local mslug, cslug = split_id(chapter_id)
  if not mslug then
    return { ok = false, error = "comizy: bad chapter id " .. chapter_id }
  end

  local bid = build_id()
  if not bid then
    return { ok = false, error = "comizy: version backend unreachable" }
  end
  local res = get_json(
    WEB
      .. "/_next/data/"
      .. bid
      .. "/"
      .. mslug
      .. "/"
      .. cslug
      .. ".json?slug="
      .. host.url_encode(mslug)
      .. "&chapter-slug="
      .. host.url_encode(cslug)
  )
  local ch = res and (res.pageProps or {}).initialChapter
  local imgs = ch and ch.images or {}
  if #imgs == 0 then
    return { ok = false, error = "comizy: no pages for " .. chapter_id }
  end
  return { ok = true, data = imgs }
end
