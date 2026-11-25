local c = require("common")
local BASE = c.BASE
local trim = c.trim
local get_json = c.get_json
local absolute = c.absolute
return function()
  local res, err = host.http.fetch(BASE .. "/tags")
  if err ~= nil or res.status ~= 200 then
    return { ok = false, error = err or "dynastyscan: tags fetch failed" }
  end
  local names, seen = {}, {}
  for _, m in ipairs(host.re2.find_all([[href="/tags/([a-z0-9_%-]+)"[^>]*>([^<]+)</]], res.body or "")) do
    local label = host.text.unescape(trim(m[3]))
    if m[2] ~= "" and label ~= "" and not seen[m[2]] then
      seen[m[2]] = true
      names[#names + 1] = label
    end
  end
  table.sort(names)
  return { ok = true, data = names }
end
