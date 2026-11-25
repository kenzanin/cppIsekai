local c = require("common")
local SITE = c.SITE
local API = c.API
local UA = c.UA
local HEADERS = c.HEADERS
local trim = c.trim
local abs_url = c.abs_url
local abs_url = c.abs_url
local tags_to_names = c.tags_to_names
local decode_id = c.decode_id
local http_get = c.http_get
local http_post_json = c.http_post_json
local chapter_pages = c.chapter_pages
return function(params)
  local manga_id = tostring(decode_id(params.id) or "")
  if manga_id == "" then
    return { ok = false, error = "comico: missing id" }
  end

  local sources = http_get(API .. "/manga/" .. host.url_encode(manga_id) .. "/sources")
  if type(sources) ~= "table" or #sources == 0 then
    -- fall back to the native catalogue rather than reporting no chapters
    sources = { { sourceId = "mangadex" } }
  end

  local chapters = {}
  for _, src in ipairs(sources) do
    local sid = type(src) == "table" and src.sourceId or nil
    local readable = type(src) == "table" and tonumber(src.readableChapterCount) or nil
    if sid and (readable == nil or readable > 0) then
      local langs =
        http_get(API .. "/manga/" .. host.url_encode(manga_id) .. "/languages?source=" .. host.url_encode(sid))
      if type(langs) ~= "table" then
        langs = {}
      end
      for _, lang in ipairs(langs) do
        if type(lang) == "string" then
          chapter_pages(
            API
              .. "/manga/"
              .. host.url_encode(manga_id)
              .. "/chapters?order=desc&sort=number&limit=100"
              .. "&source="
              .. host.url_encode(sid)
              .. "&language="
              .. host.url_encode(lang),
            chapters
          )
        end
      end
    end
  end

  -- newest first across every source and language (source parity)
  table.sort(chapters, function(a, b)
    return tostring(a.release_at or "") > tostring(b.release_at or "")
  end)
  return { ok = true, data = chapters }
end
