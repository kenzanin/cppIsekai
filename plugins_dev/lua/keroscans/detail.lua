-- Detail: GET /series/comic/<slug>. Scalars from the facts <dl>; genres from
-- the JSON-LD block (no parser needed -- bracket slice + quoted-token scan).
local DT = [[<dt>]] -- anchor per fact row: <dt>Label</dt><dd>Value</dd>

local function fact(body, label)
  local pat = [[<dt>]] .. label .. [[</dt><dd>([^<]*)</dd>]]
  local m = host.re2.find(pat, body)
  return m and host.text.trim(host.text.unescape(m[2] or "")) or ""
end

return function(ctx)
  return function(params)
    local slug = tostring(params.id or "")
    if slug == "" then
      return { ok = false, error = "keroscans: detail needs id" }
    end
    local body, err = ctx.get(ctx.BASE .. "/series/comic/" .. slug)
    if not body then
      return { ok = false, error = err }
    end

    local name = host.text.trim(host.text.unescape(body:match('property="og:title" content="([^"]*)"') or ""))
    local cover = body:match('property="og:image" content="([^"]*)"') or ""
    -- og:description is truncated with "..."; the synopsis <p> holds the full text.
    local syn = body:match([[<p[^>]*class="[^"]*ks%-sp__clamp[^"]*"[^>]*>(.-)</p>]])
    local description = syn and host.text.trim(host.text.unescape((syn:gsub("<[^>]+>", " ")))) or ""
    if description == "" then
      description = host.text.trim(host.text.unescape(body:match('property="og:description" content="([^"]*)"') or ""))
    end

    local tags = {}
    local ld = body:match('"genre":%[([^%]]*)%]')
    if ld then
      for tag in ld:gmatch('"([^"]*)"') do
        if tag ~= "" then
          tags[#tags + 1] = tag
        end
      end
    end

    local authors, artists = {}, {}
    for name_part in (fact(body, "Author") .. ","):gmatch("([^,]+),") do
      local n = host.text.trim(name_part)
      if n ~= "" then
        authors[#authors + 1] = n
      end
    end
    for name_part in (fact(body, "Artist") .. ","):gmatch("([^,]+),") do
      local n = host.text.trim(name_part)
      if n ~= "" then
        artists[#artists + 1] = n
      end
    end

    if name == "" then
      return { ok = false, error = "keroscans: no og:title for " .. slug }
    end
    return {
      ok = true,
      data = {
        id = slug,
        name = name,
        cover = cover,
        status = host.text.normalize_status(fact(body, "Status")),
        tags = tags,
        release_at = 0,
        authors = authors,
        artists = artists,
        description = description,
        url = ctx.BASE .. "/series/comic/" .. slug,
      },
    }
  end
end
