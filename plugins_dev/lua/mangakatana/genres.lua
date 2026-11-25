-- Site genre list; kept separate so main.lua stays logic-only.
-- Slugs as they appear in /genre/<slug>; scraped from the site nav (goIsekai verbatim).
local GENRES = {
  { name = "Action", slug = "action" },
  { name = "Adventure", slug = "adventure" },
  { name = "Comedy", slug = "comedy" },
  { name = "Drama", slug = "drama" },
  { name = "Fantasy", slug = "fantasy" },
  { name = "Harem", slug = "harem" },
  { name = "Historical", slug = "historical" },
  { name = "Horror", slug = "horror" },
  { name = "Isekai", slug = "isekai" },
  { name = "Josei", slug = "josei" },
  { name = "Martial Arts", slug = "martial-arts" },
  { name = "Mecha", slug = "mecha" },
  { name = "Mystery", slug = "mystery" },
  { name = "Psychological", slug = "psychological" },
  { name = "Reincarnation", slug = "reincarnation" },
  { name = "Romance", slug = "romance" },
  { name = "School Life", slug = "school-life" },
  { name = "Sci-Fi", slug = "sci-fi" },
  { name = "Seinen", slug = "seinen" },
  { name = "Shoujo", slug = "shoujo" },
  { name = "Shounen", slug = "shounen" },
  { name = "Slice of Life", slug = "slice-of-life" },
  { name = "Sports", slug = "sports" },
  { name = "Super Power", slug = "super-power" },
  { name = "Supernatural", slug = "supernatural" },
  { name = "Tragedy", slug = "tragedy" },
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
