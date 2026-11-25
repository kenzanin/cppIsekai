local c = require("common")
local BASE = c.BASE
local dle = c.dle
local site_get = c.site_get
local json_data = c.json_data
local abs_url = c.abs_url
local path_of = c.path_of
local manga_id_of = c.manga_id_of
local parse_readed_cards = c.parse_readed_cards
local genre_ids = c.genre_ids
local total_memo = c.total_memo
local pager_pages = c.pager_pages
local remember = c.remember
local load_genres = c.load_genres
return function(params)
  local reader = params.id or ""
  if reader == "" then
    return { ok = true, data = {} }
  end
  local body = site_get(reader)
  if not body or body == "" then
    return { ok = false, error = "batcave: reader fetch failed: " .. reader }
  end
  local data = json_data(body)
  if not data then
    return { ok = false, error = "batcave: no __DATA__ in reader: " .. reader }
  end
  local images = data.images or {}
  local pages = tonumber(data.pages) or #images
  if pages > #images then
    pages = #images
  end
  local out = {}
  for i = 1, pages do
    if images[i] then
      out[#out + 1] = images[i]
    end
  end
  if #out == 0 then
    return { ok = false, error = "batcave: no pages for " .. reader }
  end
  return { ok = true, data = out }
end

-- ── genres ─────────────────────────────────────────────────────────────────
-- GET /comix/ once to grab __XFILTER__'s "g" list (genre id -> name).
