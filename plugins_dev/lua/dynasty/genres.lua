local c = require("common")
local BASE = c.BASE
local OPTS = c.OPTS
local http_req = c.http_req
local find1 = c.find1
local clean = c.clean
local first = c.first
local get_json = c.get_json
local json_url = c.json_url
local split_id = c.split_id
local tags_of = c.tags_of
return function()
  local body = http_req("/search?q=") or ""
  local out, seen = {}, {}
  for _, href in ipairs(host.html.attr(body, "a[href^='/tags/']", "href") or {}) do
    local slug = find1(href, [[^/tags/([^/?#]+)]])
    if slug and not seen[slug] then
      seen[slug] = true
      out[#out + 1] = clean(slug:gsub("_", " "))
    end
  end
  return { ok = true, data = out }
end
