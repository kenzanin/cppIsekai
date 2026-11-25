local c = require("common")
local BASE = c.BASE
local UA = c.UA
local LIMIT = c.LIMIT
local STATUS = c.STATUS
local get = c.get
local txt = c.txt
local cards = c.cards
return function(params)
  local id = params.id or ""
  if id == "" then
    return { ok = false, error = "weebcentral: missing chapter id" }
  end
  local body, err = get("/chapters/" .. host.url_encode(id) .. "/images")
  if err then
    return { ok = false, error = err }
  end
  local out = {}
  for src in body:gmatch('<img%s+src="(https?://[^"]+)"') do
    if not src:find("broken_image", 1, true) then
      out[#out + 1] = src
    end
  end
  if #out == 0 then
    return { ok = false, error = "weebcentral: no page images for " .. id }
  end
  return { ok = true, data = out }
end
