local c = require("common")
local BASE = c.BASE
local dle = c.dle
local find1 = c.find1
local num_str = c.num_str
local site_get = c.site_get
local abs_url = c.abs_url
local path_of = c.path_of
local manga_id_of = c.manga_id_of
local parse_cd_cards = c.parse_cd_cards
local normalize_status = c.normalize_status
local genre_map = c.genre_map
return function(params)
  local id = tostring(params.id or "")
  local body = site_get(path_of(id))
  if body == "" then
    return { ok = false, error = "mangabay: detail fetch failed" }
  end

  local title = find1(body, [[<header class="page__header">\s*<h1>([^<]+)</h1>]])
    or find1(body, [[<title>([^<]+)</title>]])
    or ""
  title = host.text.trim((host.text.unescape(title):gsub("%s*Manga%-?Bay.*$", "")))

  local alt_titles = find1(body, [[<p class="page__alt">(.*?)</p>]]) or ""

  -- the poster <img> is sometimes wrapped in an <a> ("Open full cover")
  local cover = find1(body, [[<div class="page__poster[^"]*"[^>]*>\s*<img[^>]*src="([^"]+)"]])
    or find1(body, [[<div class="page__poster[^"]*"[^>]*>\s*<a[^>]*>\s*<img[^>]*src="([^"]+)"]])
  cover = abs_url(cover)

  local desc = find1(body, [[<div class="page__desc-text full-text clearfix">(.*?)</div>]]) or ""
  desc = host.text.unescape(host.text.strip_html(desc))

  local author = find1(body, [[<dt>Author</dt><dd>([^<]+)</dd>]]) or ""
  local artist = find1(body, [[<dt>Artist</dt><dd>([^<]+)</dd>]]) or ""
  if author == "" then
    author = artist
  end

  local status = normalize_status(find1(body, [[<dt>Status</dt><dd>([^<]+)</dd>]]))

  local genres = {}
  local genre_row = find1(body, [[<dt>Genres</dt><dd>(.*?)</dd>]]) or ""
  for _, r in ipairs(host.re2.find_all([[<a[^>]*>([^<]+)</a>]], genre_row)) do
    local label = host.text.trim(r[2] or "")
    if label ~= "" then
      genres[#genres + 1] = label
    end
  end
  if #genres == 0 then -- JSON-LD ComicSeries fallback
    local ld = dle.comic_series(body)
    if ld and type(ld.genre) == "string" then
      for graw in ld.genre:gmatch("([^,]+)") do
        local g = host.text.trim(graw)
        if g ~= "" then
          genres[#genres + 1] = g
        end
      end
    end
  end

  -- the alt-title line is the only place the other names live; keep it as a
  -- description tail (the ABI has one description field)
  alt_titles = host.text.trim(host.text.unescape(alt_titles))
  if alt_titles ~= "" then
    desc = desc .. "\n\nAlternative titles: " .. alt_titles
  end

  return {
    ok = true,
    data = {
      id = id,
      name = title,
      cover = cover,
      status = status,
      tags = genres,
      release_at = 0, -- detail markup has no machine-readable series date
      description = desc,
      authors = author ~= "" and { author } or {},
      url = BASE .. path_of(id),
    },
  }
end
