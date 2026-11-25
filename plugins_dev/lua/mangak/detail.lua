local c = require("common")
local API = c.API
local SITE = c.SITE
local OPTS = c.OPTS
local get_json = c.get_json
local find1 = c.find1
local names = c.names
local epoch = c.epoch
local ensure_genres = c.ensure_genres
local slug_of = c.slug_of
return function(params)
  local id = slug_of(params.id or "")
  if id == "" then
    return { ok = true, data = {} }
  end
  local data, err = get_json("/titles/" .. host.url_encode(id))
  if not data then
    return { ok = false, error = err }
  end
  local t = data.title or {}
  return {
    ok = true,
    data = {
      id = t.id or id,
      name = host.text.unescape(t.name or ""),
      cover = t.cover or "",
      description = host.text.unescape(t.summary or ""),
      status = host.text.normalize_status(t.status or ""),
      tags = names(t.genres),
      authors = names(t.authors),
      artists = names(t.artists),
      release_at = epoch(t.release_date),
      url = SITE .. (t.url or ("/manga/" .. id)),
    },
  }
end
