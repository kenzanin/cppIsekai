local c = require("common")
local BASE = c.BASE
local AJAX = c.AJAX
local OPTS = c.OPTS
local http_req = c.http_req
local find1 = c.find1
local clean = c.clean
local first = c.first
local manga_url = c.manga_url
local parse_cards = c.parse_cards
return function()
  local body = http_req(BASE .. "/manga/?m_orderby=views") or ""
  local out, seen = {}, {}
  for _, g in ipairs(host.html.select(body, ".genres a") or {}) do
    g = clean(g)
    if g ~= "" and not seen[g] then
      seen[g] = true
      out[#out + 1] = g
    end
  end
  return { ok = true, data = out }
end
