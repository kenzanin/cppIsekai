local c = require("common")
local BASE = c.BASE
local UA = c.UA
local LIMIT = c.LIMIT
local STATUS = c.STATUS
local get = c.get
local txt = c.txt
local cards = c.cards
return function(_)
  local body, err = get("/search")
  if err then
    return { ok = true, data = {} }
  end
  local seen, out = {}, {}
  for value in body:gmatch('<input type="hidden" form="advanced%-search%-form" id="tag%-[^"]*" value="([^"]+)"') do
    if not seen[value] then
      seen[value] = true
      out[#out + 1] = value
    end
  end
  table.sort(out)
  return { ok = true, data = out }
end
