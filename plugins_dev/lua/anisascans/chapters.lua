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
  if slug == "" then return { ok = true, data = {} } end
  -- Madara chapters via POST /manga/<slug>/ajax/chapters/.
  local body, err = http_req(BASE .. "/manga/" .. slug .. "/ajax/chapters/", {
    method = "POST",
    body = "",
    headers = { ["X-Requested-With"] = "xmlhttprequest", ["Content-Type"] = "application/x-www-form-urlencoded" },
  })
  if err ~= nil then return { ok = false, error = err } end

  local out, seen = {}, {}
  for _, m in ipairs(find_all(body, [[(?s)<li[^>]*class="wp-manga-chapter[^"]*"[^>]*>(.*?)</li>]])) do
    local li = m[1]
    local href = find1(li, [[href="([^"]+)"]])
    local label = trim(find1(li, [[>([^<]*[Cc]hapter[^<]*)<]]) or "")
    if href and label ~= "" then
      local cid = find1(href, [[/manga/[^/]+/([^/]+)/?$]]) or find1(href, [[/([^/]+)/?$]])
      if cid and not seen[cid] then
        seen[cid] = true
        out[#out + 1] = {
          id = slug .. ":" .. cid, -- composite slug:chapterId, pages needs both
          number = chapter_num(label),
          title = label,
          release_at = 0, -- Madara dates are human strings; host has no epoch contract
          pages = 0,
        }
      end
    end
  end

  -- fallback: chapter links in the page body
  if #out == 0 then
    for _, m in ipairs(find_all(body, [[(?s)<a[^>]*href="([^"]*chapter-[^"]+)"[^>]*>\s*([^<]+)\s*</a>]])) do
      local href, label = m[2], m[3]
      local cid = find1(href, [[/([^/]+)/?$]])
      if cid and not seen[cid] then
        seen[cid] = true
        out[#out + 1] = { id = slug .. ":" .. cid, number = chapter_num(label), title = trim(label), release_at = 0, pages = 0 }
      end
    end
  end

  table.sort(out, function(a, b) return (a.number or 0) < (b.number or 0) end)
  return { ok = true, data = out }
end
