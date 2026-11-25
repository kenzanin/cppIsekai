local c = require("common")
local DESKTOP = c.DESKTOP
local MOBILE = c.MOBILE
local util = c.util
local genres = c.genres
local name_to_slug = c.name_to_slug
local manga_html = c.manga_html
return function(params)
  local chapter_id = (tostring(params.id or ""):gsub(":", "/"))
  if chapter_id == "" then
    return { ok = false, error = "fanfox: bad chapter id" }
  end
  local urls = util.get_page_urls(chapter_id)
  if urls == nil or #urls == 0 then
    return { ok = false, error = "fanfox: no pages for " .. chapter_id }
  end
  return { ok = true, data = urls }
end
