local c = require("common")
local BASE = c.BASE
local genres = c.genres
local util = c.util
local MAX_RESULT_PAGES = c.MAX_RESULT_PAGES
local http_req = c.http_req
return function(params)
  local id = tostring(params.id or "")
  local body, err = http_req(BASE .. "/manga/" .. id)
  if err ~= nil then
    return { ok = false, error = err }
  end
  return { ok = true, data = util.parse_manga_detail(body, id, BASE) }
end
