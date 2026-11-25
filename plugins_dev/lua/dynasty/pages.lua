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
return function(params)
  local dir, permalink = split_id(params.id)
  if not permalink then
    permalink = tostring(params.id or "")
    dir = "chapters"
  end
  local doc, err = get_json(json_url(dir, permalink))
  if not doc then return { ok = false, error = err } end
  local out = {}
  for _, p in ipairs(doc.pages or {}) do
    if p.url and p.url ~= "" then out[#out + 1] = host.url_abs(BASE, clean(p.url)) end
  end
  return { ok = true, data = out }
end
