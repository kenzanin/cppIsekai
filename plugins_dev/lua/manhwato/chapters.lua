local c = require("common")
local SITE = c.SITE
local get = c.get
local detail_body = c.detail_body
local find1 = c.find1
local find_all = c.find_all
local clean = c.clean
local epoch = c.epoch
local cards = c.cards
local last_page = c.last_page
local genre_slug = c.genre_slug
local page_cache = c.page_cache
local page_info = c.page_info
return function(params)
  local slug = tostring(params.id or "")
  if slug == "" then
    return { ok = true, data = {} }
  end
  local body, err = detail_body(slug)
  if not body then
    return { ok = false, error = err or "manhwato: chapters failed" }
  end
  local out = {}
  for _, m in
    ipairs(find_all(body, [[(?s)<li data-ch="\d+".*?<a href="/manhwa/[^"]+/chapter-([\d.]+)-ch(\d+)" title="([^"]*)"]]))
  do
    local num, chid, title = tonumber(m[2]) or 0, m[3], clean(m[4])
    if chid then
      out[#out + 1] = {
        id = slug .. ":" .. chid,
        number = num,
        title = title,
        release_at = epoch(
          find1(body, [[(?s)chapter-]] .. tostring(num) .. [[-ch]] .. chid .. [[[^>]*>.*?<time datetime="([^"]+)"]])
        ),
        pages = 0,
        url = SITE .. "/manhwa/" .. slug .. "/chapter-" .. tostring(num) .. "-ch" .. chid,
      }
    end
  end
  table.sort(out, function(a, b)
    return a.number < b.number
  end)
  return { ok = true, data = out }
end
