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
  local composite = tostring(params.id or "")
  local id, slug = composite:match("^([^:]+):(.+)$")
  if not id or not slug then
    return { ok = true, data = {} }
  end
  local body, err = http_req(manga_url(id) .. host.url_encode(slug) .. "/")
  if not body or body == "" then
    return { ok = false, error = err }
  end
  local urls = host.html.attr(body, "img.wp-manga-chapter-img", "src") or {}
  if #urls == 0 then
    urls = host.html.attr(body, "img.wp-manga-chapter-img", "data-src") or {}
  end
  local out = {}
  for _, u in ipairs(urls) do
    u = clean(u)
    if u ~= "" then
      out[#out + 1] = host.url_abs(BASE, u)
    end
  end
  return { ok = true, data = out }
end
