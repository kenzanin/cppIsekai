-- MangaBay (manga-bay.biz): DataLife Engine behind a PoW "guard"; search and
-- ComicList render a.cd cards, detail/reader pages carry window.__DATA__,
-- genres hide in __XFILTER__ on /comix/. Port of goIsekai
-- examples/plugins/lua/mangabay; host.re2.* replaces host.regex.* (pattern
-- first), envelope { ok, data | error }, canonical fields per the mangadex
-- plugin. Guard solving and balanced-JSON extraction live in dle.lua.
local BASE = "https://manga-bay.biz" -- manifest site_url/params, pinned like the other house ports

local dle = require("dle")

-- first capture group of the first match; nil when no match (host re2.capture).
local function find1(s, pattern)
  return host.re2.capture(pattern, tostring(s or ""))
end

-- JSON numbers decode to Lua floats, so tostring renders ids as "991476.0".
-- The site's reader paths are integral (/reader/15840/991476); the float form
-- 404s, which the host surfaces as an empty page list ("unknown chapter").
local function num_str(v)
  if type(v) ~= "number" then
    return tostring(v or "")
  end
  if v == math.floor(v) then
    return string.format("%d", v)
  end
  return tostring(v)
end

-- site_get fetches path via dle.get (guard solved over HTTP). dle.get
-- concatenates site .. path, so absolute URLs reduce to their path part.
local function site_get(path)
  if path:match("^https?://") then
    path = path:match("^https?://[^/]+(/.*)$") or "/"
  end
  return dle.get(BASE, path)
end
-- abs_url: host.url_abs anchored at BASE (D6).
local abs_url = function(path)
  return host.url_abs(BASE, path)
end

-- path_of turns a manga id into a site path; the id is the slash-free DLE
-- slug ("11222-dungeon-meshi.html"), full permalinks accepted too.
local function path_of(id)
  id = tostring(id or "")
  if id:match("^https?://") then
    return id:match("^https?://[^/]+(/.*)$") or "/"
  end
  if id:sub(1, 1) ~= "/" then
    return "/" .. id
  end
  return id
end

-- manga_id_of strips the leading slash: host routes carry one path segment.
local function manga_id_of(href)
  return path_of(href):sub(2)
end

-- search/ComicList card (verified in the source HAR):
--   <a href="/NNN-slug.html" class="cd" title="Title">
--     <span class="cd__ph "><img data-src="/uploads/..webp" alt="Title">
local function parse_cd_cards(body)
  local items, seen = {}, {}
  local rows = host.re2.find_all(
    [[(?s)<a\s+href="([^"]+)"\s+class="cd"\s+title="([^"]*)">\s*<span class="cd__ph[^"]*">\s*<img[^>]*data-src="([^"]+)"]],
    body
  )
  for _, r in ipairs(rows) do
    local href, title, data_src = r[2], r[3], r[4]
    local id = dle.slug_id(href) and manga_id_of(href) or nil
    if id and not seen[id] then
      seen[id] = true
      items[#items + 1] = { id = id, name = host.text.unescape(title), cover = abs_url(data_src) }
    end
  end
  return items
end

-- the site's "Release type" wording -> host vocabulary (host.text.normalize_status
-- covers every key here, incl. on-hold/not-yet-published; empty -> "unknown").
local normalize_status = host.text.normalize_status

-- name -> DLE genre id, filled by methods.genres; search resolves UI names.
local genre_map = nil

-- ── search ─────────────────────────────────────────────────────────────────

return {
  BASE = BASE,
  dle = dle,
  find1 = find1,
  num_str = num_str,
  site_get = site_get,
  abs_url = abs_url,
  path_of = path_of,
  manga_id_of = manga_id_of,
  parse_cd_cards = parse_cd_cards,
  normalize_status = normalize_status,
  genre_map = genre_map,
}
