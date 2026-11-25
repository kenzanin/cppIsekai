local c = require("common")
local BASE = c.BASE
local CDN_COVER = c.CDN_COVER
local XHR = c.XHR
local http_req = c.http_req
local find1 = c.find1
local search_json = c.search_json
local cover_from = c.cover_from
local lookup_author = c.lookup_author
local parse_genre_cards = c.parse_genre_cards
local genres_scrape = c.genres_scrape
return function()
  local g = genres_scrape()
  local names = {}
  for _, g2 in ipairs(g.list) do
    names[#names + 1] = g2.name
  end
  table.sort(names)
  return { ok = true, data = names }
end
