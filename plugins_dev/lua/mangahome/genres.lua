-- genres.lua — MangaHome directory slugs; each slug doubles as a browse path
-- (/action?name=<q>).
local GENRES = {
  { name = "Action", slug = "action" },
  { name = "Adventure", slug = "adventure" },
  { name = "Comedy", slug = "comedy" },
  { name = "Doujinshi", slug = "doujinshi" },
  { name = "Drama", slug = "drama" },
  { name = "Ecchi", slug = "ecchi" },
  { name = "Fantasy", slug = "fantasy" },
  { name = "Harem", slug = "harem" },
  { name = "Historical", slug = "historical" },
  { name = "Horror", slug = "horror" },
  { name = "Josei", slug = "josei" },
  { name = "Mature", slug = "mature" },
  { name = "Mecha", slug = "mecha" },
  { name = "Mystery", slug = "mystery" },
  { name = "Psychological", slug = "psychological" },
  { name = "Romance", slug = "romance" },
  { name = "Sci-fi", slug = "sci-fi" },
  { name = "Seinen", slug = "seinen" },
  { name = "Shoujo", slug = "shoujo" },
  { name = "Shounen", slug = "shounen" },
  { name = "Sports", slug = "sports" },
  { name = "Supernatural", slug = "supernatural" },
  { name = "Tragedy", slug = "tragedy" },
  { name = "Yaoi", slug = "yaoi" },
  { name = "Yuri", slug = "yuri" },
  { name = "Manga", slug = "manga" },
  { name = "Manhwa", slug = "manhwa" },
  { name = "Manhua", slug = "manhua" },
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
