local c = require("common")
local BASE = c.BASE
local trim = c.trim
local cap = c.cap
local http_body = c.http_body
local fetch_detail_body = c.fetch_detail_body
return function(params)
  local slug = tostring(params.id or "")
  local body = fetch_detail_body(slug)
  if not body then
    return { ok = true, data = {} }
  end

  local chapters, seen = {}, {}
  for _, row in ipairs(host.re2.find_all([[(?s)<li data-num="([^"]+)"[^>]*>(.*?)</li>]], body)) do
    local num, li = row[2], row[3]
    local href = cap([[href="([^"]+)"]], li)
    if href and not cap([[lockedChapterModal]], li, 1) then
      local label = trim(cap([[(?s)chapternum[^>]*>\s*([^<]+)]], li) or "")
      label = trim(host.re2.replace([[\s+]], " ", label))
      if label == "" then label = "Chapter " .. num end
      local date = trim(cap([[(?s)chapterdate[^>]*>\s*([^<]+)]], li) or "")
      local cid = cap([[/([^/]+)/?$]], href)
      if cid and not seen[cid] then
        seen[cid] = true
        chapters[#chapters + 1] = {
          id = cid,
          number = host.text.chapter_num(label) or 0,
          title = host.text.unescape(label),
          -- "January 6, 2021"-style; host.text.date_to_iso parses it
          release_at = host.text.date_to_iso(date) or 0,
          pages = 0,
        }
      end
    end
  end
  return { ok = true, data = chapters }
end
