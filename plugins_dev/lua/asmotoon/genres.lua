local c = require("common")
local BASE = c.BASE
local OPTS = c.OPTS
local http_req = c.http_req
local find1 = c.find1
local clean = c.clean
local first = c.first
local status_of = c.status_of
local series_url = c.series_url
return function()
  local body = http_req(BASE .. "/?browse=1") or ""
  local out, seen = {}, {}
  for _, g in ipairs(host.html.select(body, "#fsel-genres a[role=option][href]") or {}) do
    g = clean(g)
    if g ~= "" and not seen[g] then
      seen[g] = true
      out[#out + 1] = g
    end
  end
  return { ok = true, data = out }
end
