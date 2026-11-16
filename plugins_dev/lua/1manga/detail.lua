-- Detail scraper. Returns function(ctx) -> handler(params).
return function(ctx)
  local t = ctx.text

  return function(params)
    local slug = params.id or ""
    local empty = { id = slug, name = "", cover = "", status = "unknown", tags = {}, release_at = 0,
      authors = {}, artists = {}, description = "", url = ctx.BASE .. "/manga/" .. slug }
    if slug == "" then return { ok = true, data = empty } end

    local body, err = ctx.get(ctx.BASE .. "/manga/" .. host.url_encode(slug))
    if not body then return { ok = false, error = err } end
    local flat = t.decomment(body)

    -- <h1> starts with the title, alt names follow in <small>.
    local name = string.match(flat, "<h1[^>]->%s*([^<]+)")
      or string.match(flat, 'property="og:title" content="([^"]*)"') or ""
    name = t.unesc(name):gsub("^Read%s+", ""):gsub("%s+Manga Online for Free$", "")

    local cover = string.match(flat, 'property="og:image" content="([^"]*)"')
      or string.match(flat, '<img src="(https://thumb%.mghcdn%.com/[^"]+)"') or ""

    local function label(key)
      return t.unesc(string.match(flat, ">" .. key .. "</span><span>([^<]*)<") or "")
    end
    local function split_names(raw)
      local names = {}
      for piece in string.gmatch(raw or "", "[^;]+") do
        local one = t.unesc(piece)
        if one ~= "" then names[#names + 1] = one end
      end
      return names
    end

    local tags, seen = {}, {}
    for tag in string.gmatch(flat, 'genre%-label">([^<]+)<') do
      local tg = t.normalize_tag(t.unesc(tag))
      if tg ~= "" and not seen[tg] then
        seen[tg] = true
        tags[#tags + 1] = tg
      end
    end

    return { ok = true, data = {
      id = slug,
      name = name,
      cover = cover,
      status = t.normalize_status(label("Status")),
      tags = tags,
      release_at = 0,
      authors = split_names(label("Author")),
      artists = split_names(label("Artist")),
      description = t.longest_paragraph(body),
      url = ctx.BASE .. "/manga/" .. slug,
    } }
  end
end
