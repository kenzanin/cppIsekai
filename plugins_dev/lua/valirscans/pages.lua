local c = require("common")
local BASE = c.BASE
local http_req = c.http_req
local find1 = c.find1
local is_abs_url = c.is_abs_url
local jsonld_book = c.jsonld_book
local flight_status = c.flight_status
return function(params)
  local composite = params.id or ""
  local manga_id, num = composite:match("^(.-):([%d.]+)$")
  if not manga_id or not num or manga_id == "" then
    return { ok = false, error = "valirscans: chapter id must be slug:N, got " .. composite }
  end
  local html, err = http_req(BASE .. "/series/comic/" .. manga_id .. "/chapter/" .. num)
  if err ~= nil then return { ok = false, error = err } end

  -- Page images are direct media.valirscans.org URLs in reading order; dedupe
  -- keeps order (a chapter cover can repeat via srcset).
  local pat = [[(https://media\.valirscans\.org/series/]]
    .. host.re2.quote(manga_id)
    .. [[/\d+/p-[^"\\]+\.webp)]]
  local out, seen = {}, {}
  for _, m in ipairs(host.re2.find_all(pat, html)) do
    if m[1] ~= "" and not seen[m[1]] then
      seen[m[1]] = true
      out[#out + 1] = m[1]
    end
  end
  if #out == 0 then return { ok = false, error = "valirscans: no pages for " .. composite } end
  return { ok = true, data = out }
end
