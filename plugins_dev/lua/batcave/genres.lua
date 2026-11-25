local c = require("common")
local BASE = c.BASE
local dle = c.dle
local site_get = c.site_get
local json_data = c.json_data
local abs_url = c.abs_url
local path_of = c.path_of
local manga_id_of = c.manga_id_of
local parse_readed_cards = c.parse_readed_cards
local genre_ids = c.genre_ids
local total_memo = c.total_memo
local pager_pages = c.pager_pages
local remember = c.remember
local load_genres = c.load_genres
return function()
  local list = load_genres()
  if #list == 0 then
    return { ok = true, data = {} }
  end
  return { ok = true, data = list }
end
