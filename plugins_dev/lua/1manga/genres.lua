-- Genres scraper. Returns function(ctx) -> handler(params).
-- Names are returned as plain strings sorted as the site shows them; the
-- name->slug map is stashed in slugmap (per-state require cache) so the
-- search handler can resolve a chip back to its site listing page.
return function(ctx)
  local t = ctx.text
  local slugmap = require("slugmap")

  return function()
    local body = ctx.get(ctx.BASE .. "/search")
    if not body then return { ok = true, data = {} } end
    local names, seen = {}, {}
    for slug, name in string.gmatch(t.decomment(body), 'genre/([%w%-]+)"[^>]*>([^<]+)<') do
      local label = t.normalize_tag(t.unesc(name))
      slugmap.genre_slug[label] = slug
      if not seen[label] then
        seen[label] = true
        names[#names + 1] = label
      end
    end
    host.log.info("1manga: found " .. tostring(#names) .. " genres")
    return { ok = true, data = names }
  end
end
