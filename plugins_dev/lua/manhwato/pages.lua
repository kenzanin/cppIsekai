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
  local composite = tostring(params.id or "")
  local slug, chid = composite:match("^([^:]+):(.+)$")
  if not slug or not chid then return { ok = true, data = {} } end
  local detail = detail_body(slug)
  if not detail then return { ok = true, data = {} } end
  local href = find1(detail, [[href="(/manhwa/[^"]*-ch]] .. chid .. [[)"]])
  if not href then return { ok = true, data = {} } end
  local body, err = get(href)
  if not body then return { ok = false, error = err or "manhwato: pages failed" } end
  local out = {}
  for _, m in ipairs(find_all(body, [[(?s)<figure class="mt-page".*?<img src="([^"]+)"]]) ) do
    if m[2] and m[2] ~= "" then out[#out + 1] = m[2] end
  end
  return { ok = true, data = out }
end
