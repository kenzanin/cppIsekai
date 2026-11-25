-- genres.lua — MangaHere genre ids for /search?genres=<id>; harvested from the
-- search-page dropdown (<a data-val="N" title="Name">); ids 19 and 29 are
-- absent from the site, so the table skips them.
local GENRES = {
  { name = "Action", slug = "1" },
  { name = "Adventure", slug = "2" },
  { name = "Comedy", slug = "3" },
  { name = "Fantasy", slug = "4" },
  { name = "Historical", slug = "5" },
  { name = "Horror", slug = "6" },
  { name = "Martial Arts", slug = "7" },
  { name = "Mystery", slug = "8" },
  { name = "Romance", slug = "9" },
  { name = "Shounen Ai", slug = "10" },
  { name = "Supernatural", slug = "11" },
  { name = "Drama", slug = "12" },
  { name = "Shounen", slug = "13" },
  { name = "School Life", slug = "14" },
  { name = "Shoujo", slug = "15" },
  { name = "Gender Bender", slug = "16" },
  { name = "Josei", slug = "17" },
  { name = "Psychological", slug = "18" },
  { name = "Slice of Life", slug = "20" },
  { name = "Sci-fi", slug = "21" },
  { name = "Ecchi", slug = "22" },
  { name = "Harem", slug = "23" },
  { name = "Shoujo Ai", slug = "24" },
  { name = "Yuri", slug = "25" },
  { name = "Mature", slug = "26" },
  { name = "Tragedy", slug = "27" },
  { name = "Yaoi", slug = "28" },
  { name = "Sports", slug = "30" },
  { name = "Adult", slug = "31" },
  { name = "One Shot", slug = "32" },
  { name = "Smut", slug = "33" },
  { name = "Mecha", slug = "34" },
  { name = "Shotacon", slug = "35" },
  { name = "Lolicon", slug = "36" },
  { name = "Webtoons", slug = "37" },
}
local GENRE_MAP = {}
for _, g in ipairs(GENRES) do
  GENRE_MAP[g.name] = g.slug
end
return {
  list = GENRES,
  map = GENRE_MAP,
  names = (function()
    local n = {}
    for _, g in ipairs(GENRES) do
      n[#n + 1] = g.name
    end
    table.sort(n)
    return n
  end)(),
}
