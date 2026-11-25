local c = require("common")
local BASE = c.BASE
local OPTS = c.OPTS
local XHR = c.XHR
local http_req = c.http_req
local find1 = c.find1
local clean = c.clean
local first = c.first
local manga_url = c.manga_url
local slug_of = c.slug_of
local parse_cards = c.parse_cards
return function(params)
  local slug = slug_of(params.id or "")
  local body, err = http_req(BASE .. "/get-chapter-list?slug=" .. host.url_encode(slug), XHR)
  if not body or body == "" then return { ok = false, error = err } end
  local ok, doc = pcall(json.decode, body)
  if not ok or type(doc) ~= "table" or doc.success ~= true then
    return { ok = false, error = "bbato: bad chapters envelope" }
  end
  local out = {}
  for _, c in ipairs(doc.data or {}) do
    if c.chapter_slug and c.chapter_slug ~= "" then
      out[#out + 1] = {
        id = slug .. ":" .. c.chapter_slug,
        number = tonumber(c.chapter_num) or 0,
        title = clean(c.chapter_name or "Chapter"),
        release_at = host.text.date_epoch(tostring(c.updated_at or "")) or 0,
        pages = 0,
      }
    end
  end
  table.sort(out, function(a, b) return a.number < b.number end)
  return { ok = true, data = out }
end
