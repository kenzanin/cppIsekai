local c = require("common")
local BASE = c.BASE
local http_req = c.http_req
local API = c.API
local slug_of = c.slug_of
local first_iso = c.first_iso
local epoch_date = c.epoch_date
return function(params)
  local composite = params.id or ""
  local slug, chid = string.match(composite, "^(.-):(.+)$")
  if not slug or not chid or slug == "" then
    return { ok = false, error = "sanascans: chapter id must be slug:chapterId, got " .. composite }
  end
  local body, err = http_req(BASE .. "/series/" .. host.url_encode(slug) .. "/" .. chid)
  if err ~= nil then return { ok = false, error = err } end

  -- Reader pages are SSR <img src> in a scroll container; the site emits a
  -- double slash after the host, so scan with plain substring finds (a re2
  -- anchor stops at the host) and rebuild the URL without the duplicate.
  local out = {}
  local anchor = 'src="https://media.sanascans.com/' -- site renamed storage.->media. (2026-11, checked live)
  local pos = string.find(body, anchor, 1, true)
  while pos do
    local ustart = pos + #anchor
    local close = string.find(body, '"', ustart, true)
    if not close then break end
    local path = string.sub(body, ustart, close - 1)
    if string.find(path, "upload/series/", 1, true) then
      local u = string.gsub("https://media.sanascans.com/" .. path, "(https://[^/]+)/+", "%1/")
      out[#out + 1] = u
    end
    pos = string.find(body, anchor, close, true)
  end
  if #out == 0 then return { ok = false, error = "sanascans: no pages for " .. composite } end
  return { ok = true, data = out }
end
