local c = require("common")
local DESKTOP = c.DESKTOP
local MOBILE = c.MOBILE
local util = c.util
local genres = c.genres
local name_to_slug = c.name_to_slug
local manga_html = c.manga_html
return function()
  -- UI contract: plain name strings (rendered as chips, sent back via
  -- ?genres=name,...; search maps name -> numeric id internally).
  local names = {}
  for _, g in ipairs(genres.list) do
    names[#names + 1] = g.name
  end
  return { ok = true, data = names }
end
