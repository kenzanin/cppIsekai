local c = require("common")
local BASE = c.BASE
local UA = c.UA
local COOKIE = c.COOKIE
local OPTS = c.OPTS
local get = c.get
local find1 = c.find1
local all = c.all
local clean = c.clean
local epoch = c.epoch
local categories = c.categories
local search_url = c.search_url
local parse_search = c.parse_search
return function(params)
  local id = clean(params.id or params.chapter_id or "")
  local slug, num = id:match("^([^:]+):([0-9]+%.?[0-9]*)$")
  if not slug then
    return { ok = true, data = {} }
  end
  local html, err = get("/comic/" .. host.url_encode(slug) .. "/" .. num)
  if not html then
    return { ok = false, error = "readcomiconline: pages failed" .. (err and (": " .. err) or "") }
  end
  local block = find1(html, [[(?s)id="reader-all"[^>]*>(.*?)</div>]]) or ""
  local out = {}
  for _, m in ipairs(all(block, [[<img[^>]+src="([^"]+)"]])) do
    local url = host.text.unescape(m[2])
    if url ~= "" then
      out[#out + 1] = url
    end
  end
  return { ok = true, data = out }
end
