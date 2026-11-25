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

-- abs_url: host.url_abs anchored at BASE (D6; madaradex semantics: //scheme,
-- leading-slash strip, empty passthrough).
local abs_url = function(path) return host.url_abs(BASE, path) end

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


-- Fwd: genre catalogue parsed once by genres() below; search resolves chip
-- names to DLE ids through it.
local genre_ids = {}
local load_genres

-- ── search ─────────────────────────────────────────────────────────────────
-- Text search: GET /search/<query>; genre browse: GET /ComicList/g=<id>;
-- both paginate as <base>/page/<N>/. The site serves 10 cards per page, which
-- does not divide the host's 12-item page, so the envelope reports the site's
-- own page count (data.pages) plus the real result total. The total needs the
-- last page's item count; it is memoised per query so only the first host page
-- request of a query pays the extra fetch. The pager is windowed (link numbers
-- plus the current page as a bare <span>), so the page count is the max over
-- both inside the pagination block -- never the requested page, which may be
-- past the end and still serve the last page's cards. Memo is process-lifetime,
-- bounded to 128 keys; the host's api_cache TTL makes items as stale as this.
local total_memo = {}

local function pager_pages(body)
    local block = body:match('(<div class="pagination.-</div></div>)') or body
    local maxp = 1
    for raw in block:gmatch("/page/(%d+)/") do
        local n = tonumber(raw)
        if n > maxp then maxp = n end
    end
    for raw in block:gmatch("<span>(%d+)</span>") do
        local n = tonumber(raw)
        if n > maxp then maxp = n end
    end
    return maxp
end

local function remember(key, value)
    if #total_memo > 128 then total_memo = {} end
    total_memo[key] = value
    return value
end
-- Genre catalogue shared by the genres action and search's name->id lookup;
-- parsed once, cached for the daemon lifetime.
local genre_list = nil
load_genres = function()
    if genre_list then return genre_list end
    genre_list = {}
    local body = site_get("/comix/")
    local f = body and body ~= "" and dle.json_data(body, "__XFILTER__ = ") or nil
    local values = f and f.filter_items and f.filter_items.g and f.filter_items.g.values or nil
    if type(values) == "table" then
        for _, v in ipairs(values) do
            -- The DLE genre id is what search feeds into /ComicList/g=<id>/;
            -- the chip carries the display name.
            if type(v.value) == "string" and v.id ~= nil then
                genre_list[#genre_list + 1] = v.value
                genre_ids[v.value] = tostring(v.id)
            end
        end
    end
    return genre_list
end

return { BASE=BASE, dle=dle, site_get=site_get, json_data=json_data, abs_url=abs_url, path_of=path_of, manga_id_of=manga_id_of, parse_readed_cards=parse_readed_cards, genre_ids=genre_ids, total_memo=total_memo, pager_pages=pager_pages, remember=remember, load_genres=load_genres }
