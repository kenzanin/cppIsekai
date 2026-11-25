local c = require("common")
local BASE = c.BASE
local trim = c.trim
local http_req = c.http_req
local find_all = c.find_all
local epoch_ms_to_secs = c.epoch_ms_to_secs
local per_page = c.per_page
local page_cache = c.page_cache
local fetch_search = c.fetch_search
local page_info = c.page_info
return function(params)
  local query = tostring(params.title or params.query or params.q or "")
  if query == "" then
    return { ok = false, error = "comik: empty query" }
  end
  local page = math.max(1, tonumber(params.page) or 1)
  local items, err = fetch_search(query, page)
  if not items then
    return { ok = false, error = err or "comik: search failed" }
  end
  local info = page_info(query, #items)
  return { ok = true, data = {
    items = items,
    total = info.total,
    pages = info.pages,
  } }
end
