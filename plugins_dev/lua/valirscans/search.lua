local c = require("common")
local BASE = c.BASE
local http_req = c.http_req
local find1 = c.find1
local is_abs_url = c.is_abs_url
local jsonld_book = c.jsonld_book
local flight_status = c.flight_status
return function(params)
  local q = params.title or params.query or params.q or ""
  local body, err = http_req(BASE .. "/api/search?q=" .. host.url_encode(q))
  if err ~= nil then
    return { ok = false, error = err }
  end
  local ok, data = pcall(json.decode, body)
  if not ok or type(data) ~= "table" or type(data.series) ~= "table" then
    return { ok = true, data = { items = {}, total = 0 } }
  end
  local items = {}
  for _, r in ipairs(data.series) do
    local cover = r.coverImage or ""
    if cover ~= "" and not is_abs_url(cover) then
      cover = BASE .. cover
    end
    items[#items + 1] = { id = r.urlSlug or r.slug, name = r.title or "", cover = cover }
  end
  return { ok = true, data = { items = items, total = #items } }
end
