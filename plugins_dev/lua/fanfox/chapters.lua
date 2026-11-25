local c = require("common")
local DESKTOP = c.DESKTOP
local MOBILE = c.MOBILE
local util = c.util
local genres = c.genres
local name_to_slug = c.name_to_slug
local manga_html = c.manga_html
return function(params)
  local manga_id = tostring(params.id or "")
  local html = manga_html(manga_id)
  if not html then
    return { ok = false, error = "fanfox: chapters failed" }
  end
  return { ok = true, data = util.parse_chapter_list(html, manga_id) }
end
