-- Genre chips: GET /genres, each chip is <a href="/series?genre%5B0%5D=<slug>">Name</a>.
-- One parse feeds both actions: `list()` for the chip row, `slug_of(name)` for
-- search, which must turn a chip label back into the site's genre slug.
local LINK = [[genre%%5B[0-9]%%5D=([%w%-]+)"[^>]*>%s*<span[^>]*>([^<]+)</span>]]

local M = { names = {}, slugs = {}, loaded = false }

-- Parse once per plugin state; /genres is a static page.
function M.load(ctx)
  if M.loaded then
    return M
  end
  local body = ctx.get(ctx.BASE .. "/genres")
  if not body then
    return M
  end
  local seen = {}
  for slug, name in string.gmatch(body, LINK) do
    local label = host.text.trim(host.text.unescape(name))
    if label ~= "" then
      M.slugs[label:lower()] = slug
      if not seen[label] then
        seen[label] = true
        M.names[#M.names + 1] = label
      end
    end
  end
  table.sort(M.names)
  M.loaded = true
  return M
end

function M.slug_of(name)
  return M.slugs[tostring(name or ""):lower()]
end

return M
