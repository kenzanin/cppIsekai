local c = require("common")
local BASE = c.BASE
local genres = c.genres
local util = c.util
local MAX_RESULT_PAGES = c.MAX_RESULT_PAGES
local http_req = c.http_req
return function(params)
  local q = tostring(params.title or params.query or params.q or "")
  local page = math.max(1, tonumber(params.page) or 1)
  local tags = params.tags or params.genres or {}
  if type(tags) == "string" then -- UI joins checked genres with commas
    local list = {}
    for name in tags:gmatch("[^,]+") do list[#list + 1] = name:match("^%s*(.-)%s*$") end
    tags = list
  end

  -- Genre browse: /genres/<slug> reuses the search card markup (util.parse_search).
  if type(tags) == "table" and #tags > 0 then
    local slug = genres.map[tags[1]] or tags[1]
    local url = BASE .. "/genres/" .. host.url_encode(slug)
    if page > 1 then url = url .. "?page=" .. tostring(page) end
    local body, err = http_req(url)
    if err ~= nil then return { ok = false, error = err } end
    local items = util.parse_search(body)
    return { ok = true, data = { items = items, total = #items } }
  end

  -- /search is client-rendered; results come from the backend fragment
  -- (10 cards per page, no paginator).
  local all, seen = {}, {}
  for p = 1, MAX_RESULT_PAGES do
    local body, err =
      http_req(BASE .. "/service/backend/search/?q=" .. host.url_encode(q) .. "&page=" .. tostring(p))
    if err ~= nil then
      if p == 1 then return { ok = false, error = err } end
      break
    end
    local added = 0
    for _, r in ipairs(util.parse_search(body)) do
      if not seen[r.id] then
        seen[r.id] = true
        all[#all + 1] = r
        added = added + 1
      end
    end
    if added == 0 then break end
  end
  return { ok = true, data = { items = all, total = #all } }
end
