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
return function(params)
  local q = clean(params.title or params.query or params.q or "")
  local page = math.max(1, tonumber(params.page) or 1)
  local url = BASE .. "/?s=" .. host.url_encode(q) .. "&post_type=wp-manga"
  if page > 1 then url = url .. "&paged=" .. page end
  local body, err = http_req(url)
  if not body or body == "" then return { ok = false, error = err } end
  return { ok = true, data = { items = parse_cards(body) } }
end
