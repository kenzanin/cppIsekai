local c = require("common")
local BASE = c.BASE
local http_req = c.http_req
local sel1 = c.sel1
return function(params)
  local q = params.title or params.query or params.q or ""
  local page = math.max(1, tonumber(params.page) or 1)
  local url = BASE .. "/?s=" .. host.url_encode(q)
  if page > 1 then
    url = url .. "&paged=" .. page
  end
  local body, err = http_req(url)
  if err ~= nil then
    return { ok = false, error = err }
  end

  -- bixbox card: .bsx > a (href /comics/slug/) + .tt title + img[src]; the
  -- three queries are index-aligned per card, as in the source.
  local hrefs = host.html.attr(body, ".bsx > a", "href")
  local titles = host.html.select(body, ".bsx .tt")
  local covers = host.html.attr(body, ".bsx > a img", "src")
  local items = {}
  for i, href in ipairs(hrefs) do
    local slug = tostring(href):match("/comics/([^/]+)/?")
    if slug then
      items[#items + 1] = { id = slug, name = titles[i] or "", cover = covers[i] or "" }
    end
  end
  return { ok = true, data = { items = items, total = #items } }
end
