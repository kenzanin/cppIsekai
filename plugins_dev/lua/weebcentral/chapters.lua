local c = require("common")
local BASE = c.BASE
local UA = c.UA
local LIMIT = c.LIMIT
local STATUS = c.STATUS
local get = c.get
local txt = c.txt
local cards = c.cards
return function(params)
  local id = params.id or ""
  if id == "" then
    return { ok = true, data = {} }
  end
  local body, err = get("/series/" .. host.url_encode(id) .. "/full-chapter-list")
  if err then
    return { ok = false, error = err }
  end
  local out = {}
  local pos = 1
  while true do
    local a, b, ch_id, chunk = body:find('href="/chapters/([A-Z0-9]+)"[^>]*>(.-)</a>', pos)
    if not a then
      break
    end
    if #ch_id == 26 then
      local label = chunk:match('<span class="">([^<]+)</span>') or chunk:match("<span[^>]*>([^<]+)</span>")
      local dt = chunk:match('datetime="([^"]+)"')
      out[#out + 1] = {
        id = ch_id,
        number = host.text.chapter_num(label or ""),
        title = txt(label),
        release_at = dt and dt:sub(1, 10) or 0,
        created_at = dt or 0,
        pages = 0,
      }
    end
    pos = b + 1
  end
  return { ok = true, data = out }
end
