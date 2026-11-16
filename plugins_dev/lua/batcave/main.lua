-- BatCave plugin (batcave.biz): DLE (DataLifeEngine) site behind a custom
-- proof-of-work "guard" (no Cloudflare), solved over plain HTTP in dle.lua.
-- Port of goIsekai examples/plugins/lua/batcave; adapted to cppIsekai host
-- tools (host.http.fetch + cookie jar opt-in, host.re2.*, json globals).
-- Envelope { ok, data | error }; canonical manga fields per the mangadex plugin.
local BASE = "https://batcave.biz"

local dle = require("dle")

-- ── helpers ────────────────────────────────────────────────────────────────

-- site_get fetches path and solves the DLE guard if present (per dle.lua).
-- dle.get concatenates site .. path, so an absolute URL must be reduced to its
-- path first or the origin doubles (https://sitehttps://site/...).
local function site_get(path)
    if path:match("^https?://") then
        path = path:match("^https?://[^/]+(/.*)$") or "/"
    end
    return dle.get(BASE, path)
end

-- json_data extracts window.__DATA__ = <json> (the reader/detail envelope).
local function json_data(html)
    return dle.json_data(html, "window.__DATA__ = ")
end

-- abs_url turns "/path" into a full URL anchored at BASE, leaving absolutes alone.
local function abs_url(path)
    if not path or path == "" then return "" end
    if path:match("^https?://") then return path end
    return BASE .. path
end

-- path_of turns a manga id into a site path. The id is the DLE slug
-- ("29706-thor-1966-2011.html"); a full permalink is accepted too.
local function path_of(id)
    id = tostring(id or "")
    if id:match("^https?://") then
        return id:match("^https?://[^/]+(/.*)$") or "/"
    end
    if id:sub(1, 1) ~= "/" then return "/" .. id end
    return id
end

-- manga_id_of reduces a permalink to the slash-free DLE id. The host routes
-- manga pages as /view/manga/{pluginID}/{mangaID} - one path segment - so an
-- id carrying a slash would split the route and 404.
local function manga_id_of(href)
    return path_of(href):sub(2)
end

-- parse_readed_cards pulls div.readed cards into {id, name, cover}.
-- Card shape (the anchor carries href BEFORE class):
--   <a href="https://site/NNN-slug.html" class="readed__img ...">
--     <img data-src="/uploads/mini/.../h.webp" ... alt="Title">
local function parse_readed_cards(body)
    local cards = {}
    local rows = host.re2.find_all(
        [[(?s)<a\s+href="([^"]+)"\s+class="readed__img[^"]*"[^>]*>\s*<img[^>]*data-src="([^"]+)"[^>]*alt="([^"]*)"]],
        body)
    for _, m in ipairs(rows) do
        local href, data_src, title = m[2], m[3], m[4]
        if href and dle.slug_id(href) then
            cards[#cards + 1] = {
                id = manga_id_of(href),
                name = host.text.unescape(title or ""),
                cover = abs_url(data_src),
            }
        end
    end
    return cards
end

local methods = {}

-- ── search ─────────────────────────────────────────────────────────────────
-- Text search: GET /search/<query>/page/<N>. Category/genre browse:
-- GET /ComicList/g=<id>/page/<N> (the id is BatCave's numeric DLE genre).
-- The page is server-rendered with cards; the embedded __COMICS__ blob is
-- malformed, so cards are regex-parsed.

methods.search = function(params)
    local query = params.title or params.query or params.q or ""
    local page = math.max(1, tonumber(params.page) or 1)
    local genres = params.tags or params.genres or {}
    if type(genres) == "string" then genres = { genres } end

    local body
    if type(genres) == "table" and #genres > 0 then
        -- Genre/category browse uses ComicList with g=<id>. First genre wins;
        -- the numeric id may arrive as the raw slug or a display name.
        local id = tonumber(tostring(genres[1]):match("(%d+)"))
        if not id then return { ok = true, data = { items = {}, total = 0 } } end
        local url = "/ComicList/g=" .. id .. "/"
        if page > 1 then url = url .. "page/" .. page .. "/" end
        body = site_get(url)
    elseif query ~= "" then
        local url = "/search/" .. host.url_encode(query)
        if page > 1 then url = url .. "/page/" .. page end
        body = site_get(url)
    else
        return { ok = true, data = { items = {}, total = 0 } }
    end

    if not body or body == "" then
        return { ok = false, error = "batcave: search fetch failed" }
    end
    local items = parse_readed_cards(body)
    return { ok = true, data = { items = items, total = #items } }
end

-- ── detail ─────────────────────────────────────────────────────────────────
-- GET /<news_id-slug>.html -> window.__DATA__ (no chapters there) + JSON-LD
-- ComicSeries as the fallback source for description/cover/genres.

methods.detail = function(params)
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

methods.chapters = function(params)
    local manga_id = params.id or ""
    local body = site_get(path_of(manga_id))
    if not body or body == "" then
        return { ok = false, error = "batcave: chapter fetch failed: " .. manga_id }
    end
    local data = json_data(body)
    if not data then
        return { ok = false, error = "batcave: no __DATA__ in chapter list: " .. manga_id }
    end
    local ch = data.chapters
    if type(ch) ~= "table" then return { ok = true, data = {} } end

    local out = {}
    for _, c in ipairs(ch) do
        if not c.broken then
            -- The id IS the reader path: pages gets it back verbatim.
            local reader = "/reader/" .. tostring(data.news_id) .. "/" .. tostring(c.id) .. (data.xhash or "")
            out[#out + 1] = {
                id = reader,
                number = tonumber(c.posi) or 0,
                title = c.title or ("Issue #" .. tostring(c.id)),
                release_at = dle.dle_date(c.date) or 0, -- dotted dates the host parser does not take
                pages = tonumber(c.pages) or 0,
            }
        end
    end
    return { ok = true, data = out }
end

-- ── pages ──────────────────────────────────────────────────────────────────
-- GET /reader/<news_id>/<chapter_id> -> window.__DATA__ .images[] (| .pages).
-- The reader truncates images to `pages`; honor that so we don't emit a
-- phantom 21st page. Images are absolute (img.batcave.biz / komacdn).

methods.pages = function(params)
    local reader = params.id or ""
    if reader == "" then return { ok = true, data = {} } end
    local body = site_get(reader)
    if not body or body == "" then
        return { ok = false, error = "batcave: reader fetch failed: " .. reader }
    end
    local data = json_data(body)
    if not data then
        return { ok = false, error = "batcave: no __DATA__ in reader: " .. reader }
    end
    local images = data.images or {}
    local pages = tonumber(data.pages) or #images
    if pages > #images then pages = #images end
    local out = {}
    for i = 1, pages do
        if images[i] then out[#out + 1] = images[i] end
    end
    if #out == 0 then return { ok = false, error = "batcave: no pages for " .. reader } end
    return { ok = true, data = out }
end

-- ── genres ─────────────────────────────────────────────────────────────────
-- GET /comix/ once to grab __XFILTER__'s "g" list (genre id -> name).

methods.genres = function()
    local body = site_get("/comix/")
    if not body or body == "" then return { ok = false, error = "batcave: genres fetch failed" } end
    local f = dle.json_data(body, "__XFILTER__ = ")
    if not f or not f.filter_items then return { ok = true, data = {} } end
    local items = f.filter_items.g
    if not items or not items.values then return { ok = true, data = {} } end
    local out = {}
    for _, v in ipairs(items.values) do
        -- The slug is the DLE genre id, which is what search feeds into
        -- /ComicList/g=<id>/.
        out[#out + 1] = { name = v.value, slug = tostring(v.id) }
    end
    return { ok = true, data = out }
end

function handle(req)
    local fn = methods[req.method]
    if fn == nil then return { ok = false, error = "unknown method " .. tostring(req.method) } end
    return fn(req.params or {})
    end
