local c = require("common")
local BASE = c.BASE
local http_req = c.http_req
local API = c.API
local slug_of = c.slug_of
local first_iso = c.first_iso
local epoch_date = c.epoch_date
return function(params)
  local q = params.title or params.query or params.q or ""
  if q == "" then
    return { ok = true, data = { items = {}, total = 0 } }
  end
  local page = math.max(1, tonumber(params.page) or 1)
  local j = host.http.get_json(
    API .. "/api/query?searchTerm=" .. host.url_encode(q) .. "&perPage=100&page=" .. page,
    { headers = { accept = "application/json" } }
  )
  if type(j) ~= "table" or type(j.posts) ~= "table" then
    return { ok = false, error = "search api failed" }
  end
  local items = {}
  for _, p in ipairs(j.posts) do
    items[#items + 1] = {
      id = p.slug or "",
      name = p.postTitle or "",
      cover = p.featuredImage or "",
    }
  end
  return { ok = true, data = { items = items, total = tonumber(j.totalCount) or #items } }
end
