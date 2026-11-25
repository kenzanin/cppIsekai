local c = require("common")
local BASE = c.BASE
local OPTS = c.OPTS
local XHR = c.XHR
local http_req = c.http_req
local find1 = c.find1
local clean = c.clean
local first = c.first
local manga_url = c.manga_url
local slug_of = c.slug_of
local parse_cards = c.parse_cards
return function()
  local body = http_req(BASE .. "/filter") or ""
  local out, seen = {}, {}
  for _, g in ipairs(host.html.select(body, ".mt-genre-chip, .genre-list a") or {}) do
    g = clean(g)
    if g ~= "" and not seen[g] and not g:match("^%d+$") then seen[g] = true; out[#out + 1] = g end
  end
  return { ok = true, data = out }
end
