local c = require("common")
local BASE = c.BASE
local helpers = c.helpers
local genres = c.genres
local find1 = c.find1
local bare_slug = c.bare_slug
local label_value = c.label_value
local epoch = c.epoch
return function(params)
  local id = tostring(params.id or "")
  local manga_id, chslug = id:match("^(.-):(.+)$")
  if not manga_id or manga_id == "" or not chslug or chslug == "" then
    return { ok = true, data = {} }
  end
  local html = helpers.http_get(BASE .. "/series/" .. manga_id .. "/" .. chslug)
  if not html then return { ok = false, error = "mangabuddy: page fetch failed" } end

  local out, seen = {}, {}
  for _, r in ipairs(host.re2.find_all([[data-src="(https://[^"/]+/[^"]*?/[\d.]+/\d+\.webp)"]], html)) do
    local u = r[2]
    if u and u ~= "" and not seen[u] then
      seen[u] = true
      out[#out + 1] = u
    end
  end
  return { ok = true, data = out }
end
