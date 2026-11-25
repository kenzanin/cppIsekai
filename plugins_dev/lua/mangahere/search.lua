local c = require("common")
local BASE = c.BASE
local util = c.util
local genres = c.genres
local http_body = c.http_body
local total_memo = c.total_memo
local id_of = c.id_of
local pager_pages = c.pager_pages
local remember = c.remember
return function(params)
  local q = tostring(params.title or params.query or params.q or "")
  local page = math.max(1, tonumber(params.page) or 1)
  local tags = params.tags or params.genres or {}
  if type(tags) == "string" then -- UI joins checked genres with commas
    local list = {}
    for name in tags:gmatch("[^,]+") do list[#list + 1] = name:match("^%s*(.-)%s*$") end
    tags = list
  end

  local url, key
  if type(tags) == "table" and #tags > 0 then
    local gid = tostring(tags[1] or "")
    gid = genres.map[gid] or gid
    url = BASE .. "/search?title=" .. host.url_encode(q) .. "&genres=" .. host.url_encode(gid) .. "&nogenres=&sort=name"
    key = "g:" .. gid
  elseif q ~= "" then
    url = BASE .. "/search?title=" .. host.url_encode(q)
    key = "q:" .. q
  else
    return { ok = true, data = { items = {}, total = 0 } }
  end
  if page > 1 then url = url .. "&page=" .. tostring(page) end

  local html = http_body(url)
  if not html or html == "" then return { ok = false, error = "mangahere: search failed" } end
  local items = util.parse_search(html)
  local maxp = pager_pages(html)
  -- the windowed pager marks the current (last) page as a bare span, so a
  -- request for the true last page reports maxp-1; restore it from page
  if #items > 0 and page == maxp + 1 then maxp = page end

  local total = total_memo[key]
  if total == nil then
    if maxp <= 1 then
      total = #items
    else
      local function count_page(p)
        local body = http_body(url:gsub("&page=%d+", "") .. (p > 1 and "&page=" .. p or ""))
        return body and #util.parse_search(body) or 0
      end
      local per = page < maxp and #items or count_page(1)
      local last = page == maxp and #items or count_page(maxp)
      total = per > 0 and (maxp - 1) * per + last or #items
    end
    total = remember(key, total)
  end
  return { ok = true, data = { items = items, total = total, pages = maxp } }
end
