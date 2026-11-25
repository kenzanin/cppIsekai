local c = require("common")
local BASE = c.BASE
local trim = c.trim
local unescape = c.unescape
local strip_html = c.strip_html
local normalize_status = c.normalize_status
local http_req = c.http_req
local chapter_num = c.chapter_num
local find_all = c.find_all
local find1 = c.find1
return function(params)
  local composite = params.id or ""
  local slug, cid = composite:match("^([^:]+):(.+)$")
  if not slug or not cid then
    return { ok = false, error = "anisascans: chapter id must be slug:chapterId, got " .. tostring(params.id) }
  end
  local body, err = http_req(BASE .. "/manga/" .. slug .. "/" .. cid .. "/")
  if err ~= nil then
    return { ok = false, error = err }
  end

  -- Madara page images carry class wp-manga-chapter-img; lazy themes put the
  -- URL in data-src, eager ones in src.
  local out = {}
  local srcs = host.html.attr(body, "img.wp-manga-chapter-img", "data-src")
  if #srcs == 0 then
    srcs = host.html.attr(body, "img.wp-manga-chapter-img", "src")
  end
  for _, s in ipairs(srcs) do
    local url = trim(s)
    if url ~= "" then
      out[#out + 1] = url
    end
  end
  if #out == 0 then
    return { ok = false, error = "anisascans: no pages for " .. composite }
  end
  return { ok = true, data = out }
end
