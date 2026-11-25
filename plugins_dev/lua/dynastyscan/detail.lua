local c = require("common")
local BASE = c.BASE
local trim = c.trim
local get_json = c.get_json
local absolute = c.absolute
return function(params)
  local slug = tostring(params.id or "")
  local data = get_json(BASE .. "/series/" .. host.url_encode(slug) .. ".json")
  if not data then
    -- keep the id so an unreachable source still reads as a series, not {}
    return {
      ok = true,
      data = {
        id = slug,
        name = "",
        cover = "",
        status = "unknown",
        tags = {},
        release_at = 0,
        description = "",
        authors = {},
        artists = {},
        url = BASE .. "/series/" .. slug,
      },
    }
  end

  -- Tag types: Author / General (genres) / Status. The top-level "type" is
  -- the container kind (Series/Anthology/Doujin) — not a status.
  local authors, tags, status = {}, {}, "unknown"
  for _, tag in ipairs(data.tags or {}) do
    if tag.type == "Author" then
      authors[#authors + 1] = tag.name or ""
    elseif tag.type == "General" then
      tags[#tags + 1] = tag.name or ""
    elseif tag.type == "Status" and status == "unknown" then
      status = host.text.normalize_status(tag.name or "")
    end
  end

  local description = host.text.strip_html(data.description or "")
  description = host.re2.replace([[\s+]], " ", description) -- collapse runs of whitespace
  return {
    ok = true,
    data = {
      id = slug,
      name = data.name or "",
      cover = absolute(data.cover),
      status = status,
      tags = tags,
      -- the series JSON carries no release date
      release_at = 0,
      description = description,
      authors = authors,
      artists = {},
      url = BASE .. "/series/" .. slug,
    },
  }
end
