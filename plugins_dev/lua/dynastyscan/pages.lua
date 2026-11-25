local c = require("common")
local BASE = c.BASE
local trim = c.trim
local get_json = c.get_json
local absolute = c.absolute
return function(params)
  local permalink = tostring(params.id or "")
  local data = get_json(BASE .. "/chapters/" .. host.url_encode(permalink) .. ".json")

  local out = {}
  for _, p in ipairs((data and data.pages) or {}) do
    local url = absolute(p.url)
    if url ~= "" then out[#out + 1] = url end
  end
  if #out == 0 then
    return { ok = false, error = "dynastyscan: no pages for " .. permalink }
  end
  return { ok = true, data = out }
end
