-- Site genre list; kept separate so main.lua stays logic-only.
-- Slugs from /genres/<slug> (site navigation), verbatim from the goIsekai source.
local GENRES = {
  { name = "Action", slug = "action" },
  { name = "Adventure", slug = "adventure" },
  { name = "Comedy", slug = "comedy" },
  { name = "Comic", slug = "comic" },
  { name = "Cooking", slug = "cooking" },
  { name = "Doujinshi", slug = "doujinshi" },
  { name = "Drama", slug = "drama" },
  { name = "Ecchi", slug = "ecchi" },
  { name = "Fantasy", slug = "fantasy" },
  { name = "Gender Bender", slug = "gender-bender" },
  { name = "Harem", slug = "harem" },
  { name = "Historical", slug = "historical" },
  { name = "Horror", slug = "horror" },
  { name = "Isekai", slug = "isekai" },
  { name = "Josei", slug = "josei" },
  { name = "Manhua", slug = "manhua" },
  { name = "Manhwa", slug = "manhwa" },
  { name = "Manga", slug = "manga" },
  { name = "Martial Arts", slug = "martial-arts" },
  { name = "Mecha", slug = "mecha" },
  { name = "Mystery", slug = "mystery" },
  { name = "One shot", slug = "one-shot" },
  { name = "Psychological", slug = "psychological" },
  { name = "Romance", slug = "romance" },
  { name = "School Life", slug = "school-life" },
  { name = "Sci-fi", slug = "sci-fi" },
  { name = "Seinen", slug = "seinen" },
  { name = "Shoujo", slug = "shoujo" },
  { name = "Shoujo Ai", slug = "shoujo-ai" },
  { name = "Shounen", slug = "shounen" },
  { name = "Shounen Ai", slug = "shounen-ai" },
  { name = "Slice of Life", slug = "slice-of-life" },
  { name = "Smut", slug = "smut" },
  { name = "Sports", slug = "sports" },
  { name = "Supernatural", slug = "supernatural" },
  { name = "Tragedy", slug = "tragedy" },
  { name = "Webtoon", slug = "webtoon" },
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
