local c = require("common")
local BASE = c.BASE
local trim = c.trim
local unescape = c.unescape
local strip_html = c.strip_html
local normalize_status = c.normalize_status
local http_req = c.http_req
local chapter_num = c.chapter_num
local find_all = c.find_all
local find1 = c.find1
return function(params)
  local slug = params.id or ""
  local empty = { id = slug, name = "", cover = "", status = "unknown", tags = {}, release_at = 0,
    description = "", authors = {}, artists = {}, url = BASE .. "/manga/" .. slug .. "/" }
  if slug == "" then return { ok = true, data = empty } end

  local body, err = http_req(BASE .. "/manga/" .. slug .. "/")
  if err ~= nil then return { ok = false, error = err } end

  local cover_block = find1(body, [[(?s)class="summary_image".*?</a>]]) or ""
  local cover = trim(find1(cover_block, [[data-src="([^"]+)"]]) or find1(cover_block, [[src="([^"]+)"]]) or "")

  local tblock = find1(body, [[(?s)class="post-title[^"]*">(.*?)</div>]]) or ""
  local title = unescape(strip_html(find1(tblock, [[(?s)<h[1-6][^>]*>(.*?)</h[1-6]>]]) or ""))

  local ablock = find1(body, [[(?s)class="author-content">(.*?)</div>]]) or ""
  local author = strip_html(ablock):gsub(",", ", ")

  local gblock = find1(body, [[(?s)class="genres-content">(.*?)</div>]]) or ""
  local tags = {}
  for _, g in ipairs(find_all(gblock, [[<a[^>]*>([^<]+)</a>]])) do
    local name = unescape(trim(g[2] or ""))
    if name ~= "" then tags[#tags + 1] = name end
  end

  local status = "unknown"
  local sblock = find1(body, [[(?s)class="post-status">(.*?)$]]) or ""
  if sblock ~= "" then
    local after = find1(sblock, [[(?s)Status\s*</h5>.*?class="summary-content">\s*([^<]+)]])
    if after then status = normalize_status(after) end
  end

  local dblock = find1(body, [[(?s)class="summary__content[^"]*">(.*?)<span\s+class="[^"]*content-readmore"]]) or ""
  if dblock == "" then dblock = find1(body, [[(?s)class="summary__content[^"]*">(.*?)</div>]]) or "" end
  local description = strip_html(dblock):gsub("%s+", " ")

  local data = empty
  data.name = title
  data.cover = cover
  data.status = status
  data.tags = tags
  data.description = description
  if author ~= "" then data.authors = { author } end
  return { ok = true, data = data }
end
