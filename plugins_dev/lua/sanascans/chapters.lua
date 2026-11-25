local c = require("common")
local BASE = c.BASE
local http_req = c.http_req
local API = c.API
local slug_of = c.slug_of
local first_iso = c.first_iso
local epoch_date = c.epoch_date
return function(params)
  local slug = params.id or ""
  if slug == "" then return { ok = true, data = {} } end
  local body, err = http_req(BASE .. "/series/" .. host.url_encode(slug))
  if err ~= nil then return { ok = false, error = err } end

  -- Chapter rows: <a href="/series/<slug>/chapter-N">...Ch. N...</a> plus a
  -- <time datetime>/<YYYY-MM-DD> sibling within the same row; anchor on the
  -- link and read the next date in a bounded tail.
  local out = {}
  for _, m in ipairs(host.re2.find_all(
    [[(?s)<a href="(/series/[^"/]+/chapter-[\d.]+)"[^>]*>(.*?)</a>(.{0,400})]], body)) do
    local path, cell, tail = m[2], m[3] or "", m[4] or ""
    local chid = string.match(path, "chapter%-[%d.]+") or string.match(path, "/([^/]+)$") or path
    -- chapter number: from the visible "Ch. N" text, else the URL tail
    local num = string.match(cell, "Ch%.%s*([%d.]+)") or string.match(chid, "([%d.]+)$")
    -- date: <time datetime> in the link or row tail, else first ISO date
    local raw_date = string.match(cell, '<time[^>]*datetime="([^"]+)"')
      or string.match(tail, '<time[^>]*datetime="([^"]+)"')
      or first_iso(tail) or first_iso(cell)
    local release_at = 0
    if raw_date then release_at = epoch_date(host.text.trim(raw_date)) end
    out[#out + 1] = {
      id = slug .. ":" .. chid,
      number = tonumber(num) or 0,
      title = host.text.trim(string.match(cell, ">%s*([^<]*)%s*<!--") or string.match(cell, ">%s*([^<]*)") or ""),
      release_at = release_at,
      pages = 0,
    }
  end
  -- sanascans lists newest first; keep document order.
  return { ok = true, data = out }
end
