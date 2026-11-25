local c = require("common")
local BASE = c.BASE
local render, find1, cover_abs = c.render, c.find1, c.cover_abs
local initial_data, relative_to_iso = c.initial_data, c.relative_to_iso
-- ── detail ─────────────────────────────────────────────────────────────────

return function(params)
  local slug = params.id or ""
  local empty = { id = slug, name = "", cover = "", status = "unknown", tags = {}, release_at = 0,
    authors = {}, artists = {}, description = "", url = BASE .. "/title/" .. slug }
  if slug == "" then return { ok = true, data = empty } end

  local body, err = render(BASE .. "/title/" .. host.url_encode(slug))
  if err ~= nil then return { ok = false, error = err } end

  local raw = initial_data(body)
  if not raw then
    host.log.error("comix: no initial-data in detail page: " .. slug)
    return { ok = false, error = "envelope_unrecognised: no initial-data" }
  end
  local ok, data = pcall(json.decode, raw)
  if not ok or type(data) ~= "table" or type(data.queries) ~= "table" then
    return { ok = false, error = "envelope_unrecognised" }
  end
  -- The detail query is keyed ["manga","detail","<hid>"].
  for k, v in pairs(data.queries) do
    if type(k) == "string" and string.find(k, '"detail"', 1, true) and type(v) == "table" then
      local genres = {}
      if type(v.genres) == "table" then
        for _, g in ipairs(v.genres) do
          if type(g) == "table" then
            local name = g.title or g.slug or g.name
            if type(name) == "string" then genres[#genres + 1] = name end
          elseif type(g) == "string" then
            genres[#genres + 1] = g
          end
        end
      end
      -- poster is {medium=..., large=...}; take the largest. Anything
      -- non-string stays "" so cover is always a string.
      local cover = ""
      if type(v.cover) == "string" then
        cover = v.cover
      elseif type(v.poster) == "string" then
        cover = v.poster
      elseif type(v.poster) == "table" then
        if type(v.poster.large) == "string" then cover = v.poster.large
        elseif type(v.poster.medium) == "string" then cover = v.poster.medium end
      end
      local d = {
        id = slug,
        name = v.title or "",
        cover = cover_abs(cover),
        status = host.text.normalize_status(type(v.status) == "string" and v.status or ""),
        tags = genres,
        release_at = 0,
        authors = (type(v.author) == "string" and v.author ~= "") and { v.author } or {},
        artists = (type(v.artist) == "string" and v.artist ~= "") and { v.artist } or {},
        description = v.synopsis or v.description or "",
        url = BASE .. "/title/" .. slug,
      }
      return { ok = true, data = d }
    end
  end
  host.log.error("comix: no detail query in initial-data: " .. slug)
  return { ok = false, error = "envelope_unrecognised: no detail query" }
end
