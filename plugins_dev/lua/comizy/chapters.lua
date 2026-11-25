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
  local mid, slug = split_id(manga_id)
  if not mid then
    return { ok = true, data = {} }
  end

  local res = get_json(API .. "/titles/" .. host.url_encode(mid) .. "/chapters")
  if not res then
    return { ok = false, error = "comizy: chapters failed" }
  end
  local out = {}
  for _, c in ipairs((res.data or {}).chapters or {}) do
    if c.slug then
      local title = c.name or c.slug
      out[#out + 1] = {
        id = slug .. "." .. c.slug,
        number = comizy_chapter_num(title) or 0,
        title = title,
        release_at = c.updated_at or 0, -- ISO string from the API, pass-through
        pages = 0,
      }
    end
  end
  return { ok = true, data = out }
end
