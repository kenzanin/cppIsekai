-- Site genre list; kept separate so main.lua stays logic-only.
local GENRES = {
  { name = "Action", slug = "39" },
  { name = "Adventure", slug = "37" },
  { name = "Comedy", slug = "6" },
  { name = "Drama", slug = "31" },
  { name = "Fantasy", slug = "36" },
  { name = "Historical", slug = "45" },
  { name = "Horror", slug = "44" },
  { name = "Martial Arts", slug = "29" },
  { name = "Mystery", slug = "32" },
  { name = "Psychological", slug = "18" },
  { name = "Romance", slug = "9" },
  { name = "Sci-Fi", slug = "1" },
  { name = "Slice of Life", slug = "7" },
  { name = "Supernatural", slug = "22" },
  { name = "Thriller", slug = "19" },
  { name = "Tragedy", slug = "5" },
}
local GENRE_MAP = {}
for _, g in ipairs(GENRES) do GENRE_MAP[g.name] = g.slug end
return { list = GENRES, map = GENRE_MAP, names = (function() local n = {} for _, g in ipairs(GENRES) do n[#n+1] = g.name end table.sort(n) return n end)() }
