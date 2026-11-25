local c = require("common")
local BASE = c.BASE
local UA = c.UA
local COOKIE = c.COOKIE
local OPTS = c.OPTS
local get = c.get
local find1 = c.find1
local all = c.all
local clean = c.clean
local epoch = c.epoch
local categories = c.categories
local search_url = c.search_url
local parse_search = c.parse_search
return function(params)
  local id = clean(params.id or params.slug or "")
  if id == "" then return { ok = true, data = {} } end
  local html, err = get("/comic/" .. host.url_encode(id))
  if not html then return { ok = false, error = "readcomiconline: chapters failed" .. (err and (": " .. err) or "") } end

  local pat = [[(?s)<a href="https://readcomicsonline\.ru/comic/]] .. id ..
              [[/([0-9]+(?:\.[0-9]+)?)"[^>]*>(.*?)</a>]]
  local out, seen = {}, {}
  for _, m in ipairs(all(html, pat)) do
    local num = tonumber(m[2])
    if num and not seen[num] then
      seen[num] = true
      local date = find1(m[3], [[text-xs text-slate-500">([^<]+)</span>]]) or ""
      out[#out + 1] = {
        id = id .. ":" .. tostring(num),
        number = num,
        title = string.format("Chapter %g", num),
        release_at = epoch(date),
        pages = 0,
      }
    end
  end
  table.sort(out, function(a, b) return a.number < b.number end)
  return { ok = true, data = out }
end
