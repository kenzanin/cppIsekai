-- MangaHub genre facet (mirrors the site's own list); GraphQL takes the
-- name directly (genre: "Action"), so slug doubles as the API form.
local GENRES = {
    { name = "Action", slug = "Action" },
    { name = "Adventure", slug = "Adventure" },
    { name = "Comedy", slug = "Comedy" },
    { name = "Cooking", slug = "Cooking" },
    { name = "Doujinshi", slug = "Doujinshi" },
    { name = "Drama", slug = "Drama" },
    { name = "Fantasy", slug = "Fantasy" },
    { name = "Gender bender", slug = "Gender Bender" },
    { name = "Harem", slug = "Harem" },
    { name = "Historical", slug = "Historical" },
    { name = "Horror", slug = "Horror" },
    { name = "Isekai", slug = "Isekai" },
    { name = "Josei", slug = "Josei" },
    { name = "Martial arts", slug = "Martial Arts" },
    { name = "Mecha", slug = "Mecha" },
    { name = "Mystery", slug = "Mystery" },
    { name = "One shot", slug = "One Shot" },
    { name = "Psychological", slug = "Psychological" },
    { name = "Romance", slug = "Romance" },
    { name = "School life", slug = "School Life" },
    { name = "Sci-fi", slug = "Sci Fi" },
    { name = "Seinen", slug = "Seinen" },
    { name = "Shoujo", slug = "Shoujo" },
    { name = "Shounen", slug = "Shounen" },
    { name = "Slice of life", slug = "Slice of Life" },
    { name = "Sports", slug = "Sports" },
    { name = "Supernatural", slug = "Supernatural" },
    { name = "Tragedy", slug = "Tragedy" },
    { name = "Webtoons", slug = "Webtoons" },
}
return { list = GENRES, names = (function()
    local n = {}
    for _, g in ipairs(GENRES) do n[#n + 1] = g.name end
    return n
end)() }
