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
  local mangaID, chapterID = (params.id or ""):match("^([^:]+):(.+)$")
  if not mangaID or not chapterID then
    return { ok = false, error = "atsumaru: chapter id must be mangaId:chapterId, got "
      .. tostring(params.id) }
  end
  local data, err = get_json(BASE .. "/api/read/chapter?mangaId=" .. host.url_encode(mangaID)
    .. "&chapterId=" .. host.url_encode(chapterID))
  if err ~= nil then return { ok = false, error = err } end
  local read = type(data) == "table" and data.readChapter
  local list = type(read) == "table" and read.pages or nil
  local out = {}
  for _, p in ipairs(type(list) == "table" and list or {}) do
    local url = image_url(type(p) == "table" and p.image or nil)
    if url ~= "" then out[#out + 1] = url end
  end
  if #out == 0 then return { ok = false, error = "atsumaru: no pages" } end
  return { ok = true, data = out }
end
