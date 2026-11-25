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
return function()
  local body, err = http_req(BASE .. MANGA_PATH)
  if err ~= nil then return { ok = false, error = err } end
  local seen, names, map = {}, {}, {}
  -- the label is not always bare text (sometimes span-wrapped): capture to </a>
  for _, m in ipairs(host.re2.find_all([[href="[^"]*/genre/([a-z0-9_-]+)/?"[^>]*>(.*?)</a>]], body)) do
    local slug, label = m[2], host.text.unescape(host.text.trim(host.text.strip_html(m[3] or "")))
    if slug ~= "" and label ~= "" and not seen[slug] then
      seen[slug] = true
      names[#names + 1] = label
      map[label] = slug
    end
  end
  table.sort(names)
  genre_map = map
  return { ok = true, data = names }
end
