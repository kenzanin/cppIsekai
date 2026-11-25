local c = require("common")
local BASE = c.BASE
local genres = c.genres
local BROWSER_UA = c.BROWSER_UA
local http_body = c.http_body
local date_epoch = c.date_epoch
return function(params)
  local composite = tostring(params.id or "")
  local slug, num = composite:match("^(.-):c?([%d.]+)$")
  if not slug or slug == "" or not num or num == "" then return { ok = true, data = {} } end

  local body = http_body(BASE .. "/manga/" .. slug .. "/c" .. num)
  if not body or body == "" then return { ok = false, error = "upstream_unavailable" } end

  local out = {}
  for raw in body:gmatch('src="([^"]+%.jpg[^"]*)"') do
    local u = raw
    if u:sub(1, 2) == "//" then u = "https:" .. u end
    if u:find("/store/manga/", 1, true) then out[#out + 1] = u end -- skips avatars
  end
  if #out == 0 then return { ok = false, error = "no_pages" } end
  return { ok = true, data = out }
end
