local c = require("common")
local BASE = c.BASE
local dle = c.dle
local find1 = c.find1
local num_str = c.num_str
local site_get = c.site_get
local abs_url = c.abs_url
local path_of = c.path_of
local manga_id_of = c.manga_id_of
local parse_cd_cards = c.parse_cd_cards
local normalize_status = c.normalize_status
local genre_map = c.genre_map
return function(params)
  local id = tostring(params.id or "")
  local body = site_get(path_of(id))
  local data = dle.json_data(body, "window.__DATA__ = ")
  local rows = data and type(data.chapters) == "table" and data.chapters or nil
  local out = {}
  if rows then
    for _, c in ipairs(rows) do
      if not c.broken then
        out[#out + 1] = {
          -- the id IS the reader path; pages receives it back verbatim
          id = "/reader/" .. num_str(data.news_id) .. "/" .. num_str(c.id),
          number = tonumber(c.posi) or host.text.chapter_num(c.title or "") or 0,
          title = c.title or ("Issue #" .. tostring(c.id)),
          release_at = dle.dle_epoch(c.date),
          pages = tonumber(c.pages) or 0,
        }
      end
    end
    table.sort(out, function(a, b) return a.number < b.number end)
  end
  return { ok = true, data = out }
end
