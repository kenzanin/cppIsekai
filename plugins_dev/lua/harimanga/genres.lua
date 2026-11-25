local c = require("common")
local BASE = c.BASE
local http_req = c.http_req
local genre_map = c.genre_map
local find1 = c.find1
return function()
  -- Scraped from the site's own nav (source's choice: a hardcoded list rots
  -- when the theme reorganises). Names for the UI; slugs memoized for search.
  local body, err = http_req(BASE .. "/")
  if err ~= nil then
    return { ok = false, error = err }
  end
  local seen, names = {}, {}
  local map = {}
  for _, m in ipairs(host.re2.find_all([[href="[^"]*harimanga\.cc/manga-genre/([a-z0-9-]+)/"[^>]*>([^<]+)</]], body)) do
    local slug, label = m[2], host.text.unescape(host.text.trim(m[3]))
    if label ~= "" and not seen[slug] then
      seen[slug] = true
      names[#names + 1] = label
      map[label] = slug
    end
  end
  table.sort(names)
  genre_map = map
  return { ok = true, data = names }
end
