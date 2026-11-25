local c = require("common")
local BASE = c.BASE
local helpers = c.helpers
local genres = c.genres
local find1 = c.find1
local bare_slug = c.bare_slug
local label_value = c.label_value
local epoch = c.epoch
return function(params)
  local manga_id = tostring(params.id or "")
  local slug = bare_slug(manga_id)
  if not slug or slug == "" then
    return { ok = true, data = {} }
  end
  local raw = helpers.http_get(BASE .. "/get-chapter-list?slug=" .. host.url_encode(slug), { json = true })
  if not raw then
    return { ok = false, error = "mangabuddy: chapter list fetch failed" }
  end
  local ok, body = pcall(json.decode, raw)
  if not ok or type(body) ~= "table" or not body.success or type(body.data) ~= "table" then
    return { ok = false, error = "mangabuddy: bad chapter payload for " .. slug }
  end

  local out = {}
  for _, ch in ipairs(body.data) do
    local cslug = ch.chapter_slug or ""
    -- chapter_num is missing on some rows; the slug still ends in the number
    local num = tonumber(ch.chapter_num) or host.text.chapter_num(cslug) or 0
    if num > 0 or cslug ~= "" then -- rows with neither carry no usable id
      out[#out + 1] = {
        id = manga_id .. ":" .. (cslug ~= "" and cslug or ("chapter-" .. tostring(num))),
        number = num,
        title = ch.chapter_name or ("Chapter " .. tostring(num)),
        release_at = epoch(ch.updated_at),
        pages = 0, -- the list endpoint carries no page count
      }
    end
  end
  table.sort(out, function(a, b)
    return a.number < b.number
  end)
  return { ok = true, data = out }
end
