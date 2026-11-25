local c = require("common")
local BASE = c.BASE
local http_req = c.http_req
local genre_map = c.genre_map
local find1 = c.find1
return function(params)
  local slug = params.id or ""
  if slug == "" then
    return { ok = true, data = {} }
  end
  -- Madara chapters via POST /manga/<slug>/ajax/chapters/.
  local body, err = http_req(BASE .. "/manga/" .. slug .. "/ajax/chapters/", {
    method = "POST",
    body = "",
    headers = { ["X-Requested-With"] = "xmlhttprequest", ["Content-Type"] = "application/x-www-form-urlencoded" },
  })
  if err ~= nil then
    return { ok = false, error = err }
  end

  local out, seen = {}, {}
  for _, m in ipairs(host.re2.find_all([[(?s)<li[^>]*class="wp-manga-chapter[^"]*"[^>]*>(.*?)</li>]], body)) do
    local li = m[1]
    local href = find1(li, [[href="([^"]+)"]])
    local label = host.text.trim(find1(li, [[>([^<]*[Cc]hapter[^<]*)<]]) or "")
    if href and label ~= "" then
      local cid = find1(href, [[/manga/[^/]+/([^/]+)/?$]]) or find1(href, [[/([^/]+)/?$]])
      if cid and not seen[cid] then
        seen[cid] = true
        out[#out + 1] = {
          id = slug .. ":" .. cid, -- composite slug:chapterId; pages needs both
          number = host.text.chapter_num(label) or 0,
          title = label,
          release_at = 0, -- Madara dates are human strings; host wants epoch ints (anisascans precedent)
          pages = 0,
        }
      end
    end
  end

  -- fallback: chapter links in the page body (theme variants without the li)
  if #out == 0 then
    for _, m in ipairs(host.re2.find_all([[<a[^>]*href="([^"]*chapter-[^"]+)"[^>]*>\s*([^<]+)\s*</a>]], body)) do
      local href, label = m[2], m[3]
      local cid = href and find1(href, [[/([^/]+)/?$]])
      if cid and not seen[cid] then
        seen[cid] = true
        out[#out + 1] = {
          id = slug .. ":" .. cid,
          number = host.text.chapter_num(label) or 0,
          title = host.text.trim(label or ""),
          release_at = 0,
          pages = 0,
        }
      end
    end
  end

  table.sort(out, function(a, b)
    return (a.number or 0) < (b.number or 0)
  end)
  return { ok = true, data = out }
end
