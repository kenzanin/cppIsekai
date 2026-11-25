local c = require("common")
local BASE = c.BASE
local trim = c.trim
local cap = c.cap
local http_body = c.http_body
local fetch_detail_body = c.fetch_detail_body
return function(params)
  local path = tostring(params.id or "")
  local body = http_body(BASE .. "/" .. path .. "/")
  if not body then
    return { ok = false, error = "en-thunderscans: reader fetch failed" }
  end

  local run = cap([[(?s)ts_reader\.run\((\{.*?\})\)]], body) or ""
  local imgblock = cap([=[(?s)"images"\s*:\s*\[(.*?)\]]=], run) or ""

  local out = {}
    for _, m in ipairs(host.re2.find_all([[(https?:[^\"]+?\.(webp|jpg|jpeg|png|avif))]], imgblock)) do
      local src = trim(host.re2.replace([[\\]], "", m[2]))
      if src ~= "" then out[#out + 1] = src end
  end
  if #out == 0 then
    return { ok = false, error = "en-thunderscans: no pages for " .. path }
  end
  return { ok = true, data = out }
end
