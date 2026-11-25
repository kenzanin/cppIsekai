-- Atsumaru plugin (atsu.moe): Typesense-backed search, plain JSON APIs, CDN images.
-- Port of goIsekai examples/plugins/lua/atsumaru; see knowledge/references/goisekai-plugin-examples.
-- Envelope { ok, data | error }; canonical manga fields per the mangadex plugin.
local BASE = "https://atsu.moe"
-- Images are not served by the origin (atsu.moe/static/... 410 Gone); cdn.atsu.moe
-- serves the same paths, so relative image paths resolve against the CDN.
local CDN = "https://cdn.atsu.moe"

local HEADERS = { accept = "application/json" }

-- Site's own category slugs; the search filter consumes genreIds:=[...].
-- Adult entries left out: the search filter already caps content rating.
local GENRE_SLUGS = require("genres").map

-- Relative image paths come in three shapes ("/static/posters/x.jpg" from search,
-- "posters/x.jpg" from payloads, already-absolute URLs); only relative need CDN.

-- (value, err) JSON fetch with the site's accept header (host envelope).

-- allChapters createdAt is epoch milliseconds (int64) -> unix seconds.


local normalize_status = host.text.normalize_status







local function image_url(path)
  if path == nil or path == "" then return "" end
  if path:match("^https?://") then return path end
  if path:sub(1, 2) == "//" then return "https:" .. path end
  path = path:gsub("^/static/", ""):gsub("^static/", "")
  return CDN .. "/static/" .. path
end

local function get_json(url)
  return host.http.get_json(url, { accept = "application/json" })
end

local function epoch_ms(v)
  return math.floor((tonumber(v) or 0) / 1000)
end

local function strip_html(s)
  return (host.text.strip_html(tostring(s or "")):gsub("%s+", " "))
end

return { BASE=BASE, CDN=CDN, HEADERS=HEADERS, GENRE_SLUGS=GENRE_SLUGS, normalize_status=normalize_status, image_url=image_url, get_json=get_json, epoch_ms=epoch_ms, strip_html=strip_html }
