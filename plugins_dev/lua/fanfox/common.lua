-- fanfox.net / m.fanfox.net (DM5 stack). Port of goIsekai
-- examples/plugins/lua/fanfox to the cppIsekai host ABI (host.http.fetch +
-- status check; all parsing and the chapterfun page walk live in util.lua).
-- Pages are plain URL strings: the per-image Referer is folded into the
-- chapterfun request and image fetches rely on the host image-proxy fallback.
local DESKTOP = "https://fanfox.net" -- search + chapterfun; mobile has no search endpoint
local MOBILE = "https://m.fanfox.net" -- detail/chapters pages

local util = require("util")
local genres = require("genres")

-- genres() objects carry the slug; a bare-name caller needs name -> id.
local name_to_slug = {}
for _, g in ipairs(genres.list) do
  name_to_slug[string.lower(g.name)] = g.slug
end


-- ── search ─────────────────────────────────────────────────────────────────
-- Desktop site only. Genre filter takes comma-separated numeric ids; the
-- genre browse passes an empty title, so an empty query is only rejected
-- when no genre was asked for either.

-- ── detail / chapters ──────────────────────────────────────────────────────
-- Both parse the mobile detail page: name, cover, status, genres, summary
-- and the full chapter list live there.



-- ── pages ──────────────────────────────────────────────────────────────────
-- Chapter ids marshal with ":" between slug and chapter; the URL path
-- wants "/" ("blue_lock:c161" -> "blue_lock/c161").

-- ── genres ─────────────────────────────────────────────────────────────────
-- Static list; objects, not bare names — the numeric slug is the payload
-- search?genres= consumes (batcave precedent).

local function manga_html(manga_id)
  if manga_id == "" then return nil end
  return util.http_body(MOBILE .. "/manga/" .. manga_id .. "/")
end

return { DESKTOP=DESKTOP, MOBILE=MOBILE, util=util, genres=genres, name_to_slug=name_to_slug, manga_html=manga_html }
