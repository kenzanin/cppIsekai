local c = require("common")
local SITE = c.SITE
local get = c.get
local find1 = c.find1
local find_all = c.find_all
local clean = c.clean
local epoch = c.epoch
local cards = c.cards
local last_page = c.last_page
local genre_slug = c.genre_slug
local page_cache = c.page_cache
local page_info = c.page_info
return function()
  local body, err = get("/manhwa-list")
  if not body then return { ok = false, error = err or "manhwato: genres failed" } end
  local out = {}
  for _, m in ipairs(find_all(body, [[href="/manhwa-genre/([a-z0-9-]+)"[^>]*>([^<]+)</a>]]) ) do
    out[#out + 1] = clean(m[3])
  end
  return { ok = true, data = out }
end
