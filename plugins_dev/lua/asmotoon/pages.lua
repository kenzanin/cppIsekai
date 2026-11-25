local c = require("common")
local BASE = c.BASE
local OPTS = c.OPTS
local http_req = c.http_req
local find1 = c.find1
local clean = c.clean
local first = c.first
local status_of = c.status_of
local series_url = c.series_url
return function(params)
  local composite = tostring(params.id or "")
  local slug, chapter = composite:match("^([^:]+):(.+)$")
  if not slug or not chapter then return { ok = true, data = {} } end
  local body, err = http_req(BASE .. "/" .. host.url_encode(chapter))
  if not body or body == "" then return { ok = false, error = err } end
  local urls = host.html.attr(body, "#strip-reader img.reader-page", "src") or {}
  if #urls == 0 then urls = host.html.attr(body, "#strip-reader img.reader-page", "data-src") or {} end
  local out = {}
  for _, u in ipairs(urls) do
    u = clean(u)
    if u ~= "" then out[#out + 1] = host.url_abs(BASE, u) end
  end
  return { ok = true, data = out }
end
