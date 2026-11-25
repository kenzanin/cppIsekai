local c = require("common")
local BASE = c.BASE
local render, find1, cover_abs = c.render, c.find1, c.cover_abs
local initial_data, relative_to_iso = c.initial_data, c.relative_to_iso
-- ── genres ─────────────────────────────────────────────────────────────────

return function()
  local body, err = render(BASE .. "/browse")
  local raw = (err == nil and body) and initial_data(body) or nil
  if not raw then return { ok = true, data = {} } end
  local ok, data = pcall(json.decode, raw)
  if not ok or type(data) ~= "table" then return { ok = true, data = {} } end
  -- Genres live under list.options.genres, each {id, label, slug}. The walk
  -- accepts any depth; label+slug presence is the entry test.
    local found = {}
    local function walk(v)
      if type(v) ~= "table" then return end
      local label = v.label or v.name
      if type(label) == "string" and type(v.slug) == "string" and #label < 40 then
        found[#found + 1] = { name = label, slug = v.slug }
        return
      end
      for _, child in pairs(v) do
        walk(child)
      end
    end
    walk(data)
  local seen, genres = {}, {}
  for _, g in ipairs(found) do
    if not seen[g.slug] then
      seen[g.slug] = true
      -- Chip payload is the plain name; the UI renders strings only.
      genres[#genres + 1] = g.name
    end
  end
  host.log.info("comix: found " .. tostring(#genres) .. " genres")
  return { ok = true, data = genres }
end
