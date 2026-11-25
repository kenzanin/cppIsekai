local c = require("common")
local BASE = c.BASE
local genres = c.genres
local BROWSER_UA = c.BROWSER_UA
local http_body = c.http_body
local date_epoch = c.date_epoch
return function(params)
  local id = tostring(params.id or "")
  if id == "" then
    return { ok = true, data = {} }
  end
  local body = http_body(BASE .. "/manga/" .. id)
  if not body or body == "" then
    return { ok = false, error = "upstream_unavailable" }
  end

  local out = {}
  local pos = body:find('/c%d+"', 1, false)
  while pos do
    local href = body:match('href="(/manga/' .. id .. '/c[%d.]+)"', math.max(1, pos - 200))
      or body:sub(math.max(1, pos - 200), pos + 60):match('href="(/manga/[%w_]+/c[%d.]+)"')
    local num = href and href:match("/c([%d.]+)$")
    if href and num then
      -- the date sits in a sibling cell; read up to the next chapter link
      local nxt = body:find('/c%d+"', pos + 6, false) or (pos + 1500)
      local row = body:sub(pos, nxt)
      local label = row:match('<span class="pc%-none">Ch%.? ?([^<]*)</span>')
        or row:match('name="[^"]*">.-<span[^>]*>([^<]*)</span>')
      local clean = label and host.text.trim(label) or ""
      out[#out + 1] = {
        id = id .. ":" .. num,
        number = tonumber(num) or 0,
        title = "Chapter " .. (clean ~= "" and clean or num),
        release_at = date_epoch(row:match('<span class="time">([^<]*)</span>')),
        pages = 0, -- the list carries no per-chapter page count
      }
      pos = nxt
    else
      pos = body:find('/c%d+"', pos + 4, false)
    end
  end

  if #out == 0 then
    return { ok = false, error = "no_chapters" }
  end
  return { ok = true, data = out }
end
