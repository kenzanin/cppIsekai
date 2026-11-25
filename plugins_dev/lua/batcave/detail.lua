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
return function(params)
    local manga_id = params.id or ""
    local empty = { id = manga_id, name = "", cover = "", status = "unknown", tags = {}, release_at = 0,
        description = "", authors = {}, artists = {}, url = BASE .. path_of(manga_id) }
    local path = path_of(manga_id)
    local body = site_get(path)
    if not body or body == "" then
        return { ok = false, error = "batcave: detail fetch failed: " .. manga_id }
    end

    local t = host.re2.find(
        [[<header class="page__header">\s*<div class="page__title-row">\s*<h1>([^<]+)</h1>]], body)
    local title = t[2] or host.re2.find([[<title>([^<]+)</title>]], body)[2] or ""
    title = host.text.trim(host.text.unescape(title):gsub("%s*BatCave.*$", ""))

    -- The poster <img> is sometimes wrapped in an <a> ("Open full cover"),
    -- so the direct child is not always the <img>.
    local cover = host.re2.find([[<div class="page__poster[^"]*"[^>]*>\s*<img[^>]*src="([^"]+)"]], body)[2]
        or host.re2.find([[<div class="page__poster[^"]*"[^>]*>\s*<a[^>]*>\s*<img[^>]*src="([^"]+)"]], body)[2]
        or host.re2.find("\"thumbnailUrl\":\"([^\"]+)\"", body)[2]
    cover = abs_url(cover)

    local desc = host.re2.find([[<div class="page__text full-text clearfix">(.*?)</div><div class="page__tags]], body)[2] or ""
    desc = host.text.strip_html(desc)
    if desc == "" then desc = host.re2.find([[<meta name="description" content="([^"]*)"]], body)[2] or "" end
    desc = host.text.unescape(desc)

    -- Writer row in ul.page__list: <li><div>Writer:</div> <a ...>Stan Lee</a></li>
    local author = host.re2.find([[<div>Writer:</div>\s*<a[^>]*>([^<]+)</a>]], body)[2] or ""

    local status_raw = host.re2.find([[Release type:</div>\s*([^<]+)</li>]], body)[2] or ""

    -- Genres: prefer the page__tags <a> labels, fall back to JSON-LD genre.
    local genres = {}
    for _, g in ipairs(host.re2.find_all(
        [[(?s)<div class="page__tags[^"]*">\s*(.*?)<div class="char-block]], body)) do
        for _, m in ipairs(host.re2.find_all([[<a[^>]*>([^<]+)</a>]], g[2] or "")) do
            local label = host.text.trim(m[2] or "")
            if label ~= "" then genres[#genres + 1] = host.text.unescape(label) end
        end
    end
    if #genres == 0 then
        local ld = dle.comic_series(body)
        if ld and type(ld.genre) == "string" then
            for g in string.gmatch(ld.genre, "([^,]+)") do
                local name = host.text.trim(g)
                if name ~= "" then genres[#genres + 1] = name end
            end
        end
    end

    local data = empty
    data.name = title
    data.cover = cover
    data.status = host.text.normalize_status(host.text.trim(status_raw))
    data.tags = genres
    data.description = desc
    if author ~= "" then data.authors = { host.text.unescape(author) } end
    return { ok = true, data = data }
end

-- ── chapters ───────────────────────────────────────────────────────────────
-- Chapters live in window.__DATA__ on the detail page: chapters[] with
-- {id, posi, pages, title, date}. posi is the chapter number (1-based asc),
-- so the array is newest-first - keep document order.

