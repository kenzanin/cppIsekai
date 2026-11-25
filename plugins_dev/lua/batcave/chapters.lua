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
  local manga_id = params.id or ""
  local body = site_get(path_of(manga_id))
  if not body or body == "" then
    return { ok = false, error = "batcave: chapter fetch failed: " .. manga_id }
  end
  local data = json_data(body)
  if not data then
    return { ok = false, error = "batcave: no __DATA__ in chapter list: " .. manga_id }
  end
  local ch = data.chapters
  if type(ch) ~= "table" then
    return { ok = true, data = {} }
  end

  local out = {}
  for _, c in ipairs(ch) do
    if not c.broken then
      -- The id IS the reader path: pages gets it back verbatim.
      local reader = "/reader/" .. tostring(data.news_id) .. "/" .. tostring(c.id) .. (data.xhash or "")
      out[#out + 1] = {
        id = reader,
        number = tonumber(c.posi) or 0,
        title = c.title or ("Issue #" .. tostring(c.id)),
        release_at = dle.dle_date(c.date) or 0, -- dotted dates the host parser does not take
        pages = tonumber(c.pages) or 0,
      }
    end
  end
  return { ok = true, data = out }
end

-- ── pages ──────────────────────────────────────────────────────────────────
-- GET /reader/<news_id>/<chapter_id> -> window.__DATA__ .images[] (| .pages).
-- The reader truncates images to `pages`; honor that so we don't emit a
-- phantom 21st page. Images are absolute (img.batcave.biz / komacdn).
