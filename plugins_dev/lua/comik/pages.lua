local c = require("common")
local BASE = c.BASE
local trim = c.trim
local http_req = c.http_req
local find_all = c.find_all
local epoch_ms_to_secs = c.epoch_ms_to_secs
local per_page = c.per_page
local page_cache = c.page_cache
local fetch_search = c.fetch_search
local page_info = c.page_info
return function(params)
  local chid = tostring(params.id or "")
  if chid == "" then
    return { ok = false, error = "comik: missing id" }
  end
  local body, err = http_req(BASE .. "/chapter/" .. chid)
  if not body then
    return { ok = false, error = err or "comik: reader fetch failed" }
  end

  local out = {}
  for u in body:gmatch('https://i%d+%.img00%.org/[^"]+%.webp') do
    out[#out + 1] = u
  end
  if #out == 0 then
    return { ok = false, error = "comik: no pages for " .. chid }
  end
  return { ok = true, data = out }
end
