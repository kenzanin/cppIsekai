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
return function(params)
  local q = clean(params.title or params.query or params.q or "")
  local page = math.max(1, tonumber(params.page) or 1)
  local url = BASE .. "/filter?keyword=" .. host.url_encode(q)
  if page > 1 then
    url = url .. "&page=" .. page
  end
  local body, err = http_req(url)
  if not body or body == "" then
    return { ok = false, error = err }
  end
  local has_next = #host.html.attr(body, ".pagination a[rel=next]", "href") > 0
  return { ok = true, data = { items = parse_cards(body), pages = page + (has_next and 1 or 0) } }
end
