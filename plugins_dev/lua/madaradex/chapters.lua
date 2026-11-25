local c = require("common")
local BASE = c.BASE
local AJAX = c.AJAX
local MANGA_PATH = c.MANGA_PATH
local auth_done = c.auth_done
local warm_auth = c.warm_auth
local http_req = c.http_req
local find1 = c.find1
local abs_url = c.abs_url
local manga_url = c.manga_url
local img_src = c.img_src
local genre_map = c.genre_map
return function(params)
  local slug = host.text.trim(tostring(params.id or ""))
  if slug == "" then return { ok = true, data = {} } end
  local body, err = http_req(manga_url(slug))
  if err ~= nil then return { ok = false, error = err } end

  local chapters, seen = {}, {}
  for _, m in ipairs(host.re2.find_all([[(?s)<li[^>]*class="wp-manga-chapter[^"]*"[^>]*>(.*?)</li>]], body)) do
    local li = m[1]
    local href = find1(li, [[href="([^"]+)"]])
    local cid = href and find1(href, [[/([^/]+)/?$]])
    local label = host.text.trim(host.text.strip_html(find1(li, [[(?s)<a[^>]*>(.*?)</a>]]) or ""))
    if href and cid and cid ~= "" and not seen[cid] then
      seen[cid] = true
      chapters[#chapters + 1] = {
        id = slug .. ":" .. cid,
        number = host.text.chapter_num(label) or 0,
        title = label,
        release_at = 0, -- Madara dates are human strings
        pages = 0,
      }
    end
  end
  table.sort(chapters, function(a, b) return (a.number or 0) < (b.number or 0) end)
  return { ok = true, data = chapters }
end
