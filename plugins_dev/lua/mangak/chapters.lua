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
  if id == "" then return { ok = true, data = {} } end
  local data, err = get_json("/titles/" .. host.url_encode(id) .. "/chapters")
  if not data then return { ok = false, error = err } end
  local out = {}
  for _, c in ipairs(data.chapters or {}) do
    if c.id and c.id ~= "" then
      out[#out + 1] = {
        id = id .. ":" .. c.id,
        number = tonumber(c.number) or 0,
        title = host.text.unescape(c.name or "Chapter"),
        release_at = epoch(c.updated_at),
        pages = 0,
        url = SITE .. (c.url or ""),
      }
    end
  end
  table.sort(out, function(a, b) return a.number < b.number end)
  return { ok = true, data = out }
end
