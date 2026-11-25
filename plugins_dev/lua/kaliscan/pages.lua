local c = require("common")
local BASE = c.BASE
local genres = c.genres
local util = c.util
local MAX_RESULT_PAGES = c.MAX_RESULT_PAGES
local http_req = c.http_req
return function(params)
  local path = tostring(params.id or ""):gsub(":", "/")
  local body, err = http_req(BASE .. "/manga/" .. path)
  if err ~= nil then return { ok = false, error = err } end
  local chapter_id = util.find1(body, [[chapterId\s*=\s*(\d+)]])
  if not chapter_id then return { ok = true, data = {} } end
  local img_body, ierr = http_req(BASE .. "/service/backend/chapterServer/?server_id=1&chapter_id=" .. chapter_id, {
    headers = { Referer = BASE .. "/manga/" .. path, ["X-Requested-With"] = "XMLHttpRequest" },
  })
  if ierr ~= nil then return { ok = false, error = ierr } end
  return { ok = true, data = util.parse_page_list(img_body) }
end
