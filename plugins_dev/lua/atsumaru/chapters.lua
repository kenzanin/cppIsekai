local c = require("common")
local BASE = c.BASE
local CDN = c.CDN
local HEADERS = c.HEADERS
local GENRE_SLUGS = c.GENRE_SLUGS
local normalize_status = c.normalize_status
local image_url = c.image_url
local get_json = c.get_json
local epoch_ms = c.epoch_ms
local strip_html = c.strip_html
return function(params)
  local id = params.id or ""
  local data, err = get_json(BASE .. "/api/manga/allChapters?mangaId=" .. host.url_encode(id))
  if err ~= nil then return { ok = false, error = err } end
  local raw = type(data) == "table" and data.chapters or {}
  local out, seen = {}, {}
  for _, c in ipairs(type(raw) == "table" and raw or {}) do
    if type(c) == "table" and c.id then
      local label = c.title or ("Chapter " .. tostring(c.number or ""))
      local num = tonumber(c.number)
        or tonumber(tostring(label):match("[%d%.]+"))
        or 0
      -- host routes pages/CBZ by chapter number, so a second release of the
      -- same number is unreachable and only duplicates the list; keep first.
      if not seen[num] then
        seen[num] = true
        out[#out + 1] = {
          id = id .. ":" .. c.id, -- composite mangaId:chapterId, pages needs both
          number = num,
          title = label,
          release_at = epoch_ms(c.createdAt),
          pages = tonumber(c.pageCount) or 0, -- read_ch rollup needs page count
        }
      end
    end
  end
  -- deterministic ascending like mangadex (order[chapter]=asc)
  table.sort(out, function(a, b) return (a.number or 0) < (b.number or 0) end)
  return { ok = true, data = out }
end
