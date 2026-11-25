local c = require("common")
local BASE = c.BASE
local GENRE_PATH = c.GENRE_PATH
local http_req = c.http_req
local find1 = c.find1
return function(params)
  local composite = params.id or ""
  local slug, cid = composite:match("^([^:]+):(.+)$")
  if not slug or not cid then
    return { ok = false, error = "mangasushi: chapter id must be slug:chapterId, got " .. tostring(params.id) }
  end
  local body, err = http_req(BASE .. "/manga/" .. slug .. "/" .. cid .. "/")
  if err ~= nil then return { ok = false, error = err } end

  -- Madara page images carry class wp-manga-chapter-img; lazy themes put the
  -- URL in data-src, eager ones in src.
  local out = {}
  local srcs = host.html.attr(body, "img.wp-manga-chapter-img", "data-src")
  if #srcs == 0 then srcs = host.html.attr(body, "img.wp-manga-chapter-img", "src") end
  for _, s in ipairs(srcs) do
    local url = host.text.trim(s)
    if url ~= "" then out[#out + 1] = url end
  end
  if #out == 0 then return { ok = false, error = "mangasushi: no pages for " .. composite } end
  return { ok = true, data = out }
end
