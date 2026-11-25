local c = require("common")
local BASE = c.BASE
local http_req = c.http_req
local find1 = c.find1
local is_abs_url = c.is_abs_url
local jsonld_book = c.jsonld_book
local flight_status = c.flight_status
return function(params)
  local manga_id = params.id or ""
  if manga_id == "" then
    return { ok = true, data = {} }
  end
  local html, err = http_req(BASE .. "/series/comic/" .. manga_id)
  if err ~= nil then
    return { ok = false, error = err }
  end

  -- Chapter rows are <a href="/series/comic/{slug}/chapter/{N}"> where N may
  -- be decimal (7.1); keep the raw token so URLs round-trip exactly. The same
  -- URL repeats for the "latest chapter" card, so dedupe by token.
  local pat = "/series/comic/" .. host.re2.quote(manga_id) .. [[/chapter/([\d.]+)]]
  local nums, seen = {}, {}
  for _, m in ipairs(host.re2.find_all(pat, html)) do
    local raw = m[2]
    if raw and raw ~= "" and not seen[raw] then
      seen[raw] = true
      nums[#nums + 1] = raw
    end
  end
  -- newest-first (descending number) per ABI convention
  table.sort(nums, function(a, b)
    return (tonumber(a) or 0) > (tonumber(b) or 0)
  end)

  local out = {}
  for _, raw in ipairs(nums) do
    out[#out + 1] = {
      id = manga_id .. ":" .. raw,
      number = tonumber(raw) or 0,
      title = "Chapter " .. raw,
      release_at = 0, -- list page carries no per-chapter dates
      pages = 0,
    }
  end
  return { ok = true, data = out }
end
