local c = require("common")
local BASE = c.BASE
local render, find1, cover_abs = c.render, c.find1, c.cover_abs
local initial_data, relative_to_iso = c.initial_data, c.relative_to_iso
-- ── search ─────────────────────────────────────────────────────────────────

return function(params)
  local query = params.title or params.query or params.q or ""
  if query == "" then
    return { ok = true, data = { items = {}, total = 0 } }
  end
  -- The browse paginates (~30/page); two pages like the source -- enough for
  -- the grid, bounded call budget. Dedupe by slug across pages.
  local out, seen = {}, {}
  local base = BASE .. "/browse?q=" .. host.url_encode(query)
  for page = 1, 2 do
    local body, err = render(base .. "&page=" .. page)
    if err ~= nil or not body or body == "" then
      break
    end
    local added = 0
    local pat = [[(?s)<a class="lrow__poster" href="/title/([^"]+)" aria-label="([^"]+)">.*?src="([^"]+)"]]
    for _, m in ipairs(host.re2.find_all(pat, body)) do
      local slug, title, cover = m[2], m[3], m[4]
      if not seen[slug] then
        seen[slug] = true
        -- aria-label is HTML-escaped; decode entities.
        out[#out + 1] = { id = slug, name = host.text.unescape(title or ""), cover = cover_abs(cover) }
        added = added + 1
      end
    end
    if added == 0 then
      break
    end
  end
  host.log.info("comix: search found " .. tostring(#out) .. " for " .. query)
  return { ok = true, data = { items = out, total = #out } }
end
