-- Search scraper. Returns function(ctx) -> handler(params).
-- Card: head (cover) + slug + title + the status tail, up to the genre <p>.
-- RE2 syntax, not Lua patterns (`%` is a literal there). The title anchor
-- sometimes carries rel="noindex nofollow", so attributes after href are
-- optional — requiring a bare ">" made the match run on into the next card.
local CARD = [[(?s)<div class="media-manga media">(.*?)<h4 class="media-heading"><a href="https://1manga\.co/manga/([^"]+)"[^>]*>([^<]*)</a>(.*?)<p class="_12-Zw">]]

return function(ctx)
  local t = ctx.text
  local slugmap = require("slugmap")

  return function(params)
    local q = tostring(params.title or params.query or params.q or "")
    local tags = params.tags or params.genres
    if type(tags) == "string" then tags = { tags } end
    if type(tags) ~= "table" then tags = {} end
    local first = tags[1]
    if type(first) == "table" then first = first.slug or first.name end

    -- A genre chip maps to the site's /genre/<lower-hyphen-name> listing; the
    -- text query keeps working on top of it via ?q=. No chip and no query => empty.
    local base
    if tostring(first or "") ~= "" then
      local label = t.normalize_tag(tostring(first))
      base = ctx.BASE .. "/genre/" .. host.url_encode(slugmap.genre_slug[label] or (label:gsub("%s+", "-")))
    elseif q ~= "" then
      base = ctx.BASE .. "/search"
    else
      return { ok = true, data = { items = {}, total = 0 } }
    end
    local qs = (q ~= "") and ("?q=" .. host.url_encode(q)) or ""
    local urls = { base .. qs, base .. "/page/2" .. qs }

    local out, seen = {}, {}
    for _, url in ipairs(urls) do
      local body = ctx.get(url)
      if not body then break end
      local added = 0
      for _, m in ipairs(host.re2.find_all(CARD, body)) do
        local head, slug, title, tail = t.decomment(m[2]), m[3], m[4], t.decomment(m[5])
        if not seen[slug] then
          seen[slug] = true
          out[#out + 1] = {
            id = slug,
            name = t.unesc(title),
            cover = string.match(head, 'src="(https://thumb%.mghcdn%.com/[^"]+)"') or "",
            status = t.normalize_status(string.match(tail, "chapters published %(([%a%-]+)%)") or ""),
          }
          added = added + 1
        end
      end
      if added == 0 then break end
    end
    -- The genre listing ignores ?q=, so a chip plus a typed query narrows here.
    if q ~= "" and tostring(first or "") ~= "" then
      local needle = q:lower()
      local kept = {}
      for _, it in ipairs(out) do
        if it.name:lower():find(needle, 1, true) then kept[#kept + 1] = it end
      end
      out = kept
    end
    host.log.info("1manga: search found " .. tostring(#out) .. " for " .. q .. " tags=" .. #tags)

    -- The site serves ~30 cards per page and this scraper walks two site pages,
    -- but the host pager asks for a fixed slice (limit, page). Slice the merged
    -- stream here instead of dumping everything: without it every host page
    -- repeated the same 60 cards and the migrate picker showed the whole catalog.
    local limit = tonumber(params.limit) or 12
    if limit < 1 then limit = 12 end
    local page = tonumber(params.page) or 1
    if page < 1 then page = 1 end
    local total = #out
    local start = (page - 1) * limit + 1
    local slice = {}
    for i = start, math.min(start + limit - 1, total) do
      slice[#slice + 1] = out[i]
    end
    return { ok = true, data = { items = slice, total = total, pages = math.ceil(total / limit) } }
  end
end
