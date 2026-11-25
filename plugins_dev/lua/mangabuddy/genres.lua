-- genres.lua — MangaBuddy genre slugs for /series?genres=<slug>; harvested
-- from the search-page checkbox markup (visible label, data-slug = param).
local GENRES = {
  { name = "Action", slug = "action" },
  { name = "Adult", slug = "adult" },
  { name = "Adventure", slug = "adventure" },
  { name = "Boys Love", slug = "boys-love" },
  { name = "Comedy", slug = "comedy" },
  { name = "Demons", slug = "demons" },
  { name = "Drama", slug = "drama" },
  { name = "Ecchi", slug = "ecchi" },
  { name = "Fantasy", slug = "fantasy" },
  { name = "Full Color", slug = "full-color" },
  { name = "Girls Love", slug = "girls-love" },
  { name = "Harem", slug = "harem" },
  { name = "Historical", slug = "historical" },
  { name = "Horror", slug = "horror" },
  { name = "Isekai", slug = "isekai" },
  { name = "Josei", slug = "josei" },
  { name = "Magic", slug = "magic" },
  { name = "Manga", slug = "manga" },
  { name = "Manhwa", slug = "manhwa" },
  { name = "Martial Arts", slug = "martial-arts" },
  { name = "Mature", slug = "mature" },
  { name = "Mystery", slug = "mystery" },
  { name = "One shot", slug = "one-shot" },
  { name = "Others", slug = "others" },
  { name = "Psychological", slug = "psychological" },
  { name = "Romance", slug = "romance" },
  { name = "School", slug = "school" },
  { name = "School life", slug = "school-life" },
  { name = "Sci-Fi", slug = "sci-fi" },
  { name = "Seinen", slug = "seinen" },
  { name = "Shoujo", slug = "shoujo" },
  { name = "Shounen", slug = "shounen" },
  { name = "Slice of Life", slug = "slice-of-life" },
  { name = "Smut", slug = "smut" },
  { name = "Sports", slug = "sports" },
  { name = "Supernatural", slug = "supernatural" },
  { name = "Thriller", slug = "thriller" },
  { name = "Tragedy", slug = "tragedy" },
  { name = "Webtoons", slug = "webtoons" },
  { name = "Yaoi", slug = "yaoi" },
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
