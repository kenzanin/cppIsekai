local c = require("common")
local BASE = c.BASE
local http_req = c.http_req
local API = c.API
local slug_of = c.slug_of
local first_iso = c.first_iso
local epoch_date = c.epoch_date
return function(params)
  local slug = params.id or ""
  local empty = {
    id = slug,
    name = "",
    cover = "",
    status = "unknown",
    tags = {},
    release_at = 0,
    description = "",
    authors = {},
    artists = {},
    url = BASE .. "/series/" .. host.url_encode(slug),
  }
  if slug == "" then
    return { ok = true, data = empty }
  end

  local body, err = http_req(BASE .. "/series/" .. host.url_encode(slug))
  if err ~= nil then
    return { ok = false, error = err }
  end

  -- Real cover: the itemProp="image" img src (full art). og:image on this
  -- site is a generated og-card with title overlay — cropped, so fallback only.
  local cover = host.text.unescape(
    (
      string.match(body, 'itemProp="image"[^>]-src="([^"]+)"')
      or string.match(body, 'property="og:image" content="([^"]+)"')
      or string.match(body, '"image":"([^"]+)"')
    ) or ""
  )
  local title = host.text.unescape(
    (string.match(body, 'property="og:title" content="([^"]+)"') or string.match(body, '"name":"([^"]*)"')) or slug
  )
  -- strip a trailing "Manga - Sana Scans" suffix if present
  title = string.match(title, "(.-)%s*[Mm]anga%s*-%s*[Ss]ana") or title
  local description = host.text.unescape(
    (string.match(body, 'property="og:description" content="([^"]*)"') or string.match(body, '"description":"([^"]*)"'))
      or ""
  )
  local status = host.text.unescape(string.match(body, '"status":"([^"]*)"') or "")

  local tags = {}
  -- (source captured the KEY "genre"/"category" instead of the value; fixed)
  for _, v in string.gmatch(body, '"(genre|category)"%s*:%s*"([^"]*)"') do
    if v ~= "" then
      tags[#tags + 1] = v
    end
  end

  local data = empty
  data.name = title
  data.cover = cover
  data.status = host.text.normalize_status(status)
  data.tags = tags
  data.description = description
  return { ok = true, data = data }
end
