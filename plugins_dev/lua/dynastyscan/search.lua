local c = require("common")
local BASE = c.BASE
local trim = c.trim
local get_json = c.get_json
local absolute = c.absolute
return function(params)
  local query = tostring(params.title or params.query or params.q or "")
  local page = tonumber(params.page) or 1
  local genres = type(params.tags) == "table" and params.tags or type(params.genres) == "table" and params.genres or {}
  local genre_mode = #genres > 0

  local url
  if genre_mode then
    local g = genres[1]
    if type(g) == "table" then
      g = g.slug or g.name
    end
    url = BASE .. "/tags/" .. host.url_encode(tostring(g)) .. "?view=groupings"
    if page > 1 then
      url = url .. "&page=" .. tostring(page)
    end
  else
    url = BASE .. "/search?q=" .. host.url_encode(query)
    if page > 1 then
      url = url .. "&page=" .. tostring(page)
    end
  end

  local res, err = host.http.fetch(url)
  if err ~= nil or res.status ~= 200 then
    return { ok = false, error = err or "dynastyscan: upstream status " .. tostring(res and res.status) }
  end
  local body = res.body or ""

  local items, seen = {}, {}
  if genre_mode then
    -- Cover cards with single-quoted attrs: <li class='span2'><a class='thumbnail'
    -- href='/series/slug'><img src=...><div class='caption'><b>TITLE</b>...
    -- The card carries the cover: 25 cards stay 1 request, no per-series fetch.
    for _, m in ipairs(host.re2.find_all([[(?s)<li class='span2'>(.*?)</li>]], body)) do
      local block = m[2]
      local slug = host.re2.find([[href='/series/([^']+)]], block)[2]
      local cover = host.re2.find([[src="([^"]+)"]], block)[2]
      local title = host.re2.find([[class='caption'><b>([^<]+)</b>]], block)[2]
      if slug and title and not seen[slug] then
        seen[slug] = true
        items[#items + 1] = {
          id = slug,
          name = host.text.unescape(trim(title)),
          cover = absolute(host.text.unescape(cover or "")),
        }
      end
    end
  else
    -- Definition lists: <dd>...<a href="/series/slug" class="name">TITLE</a>.
    -- Whole-entry match keeps title paired with the right slug when titles
    -- repeat. Search cards carry no <img> at all, so the cover comes from the
    -- series JSON — the same response detail reads (N+1 by site design).
    for _, m in ipairs(host.re2.find_all([[(?s)<dd>(.*?)</dd>]], body)) do
      local block = m[2]
      local slug = host.re2.find([[href="/series/([a-z0-9_]+)"]], block)[2]
      local title = host.re2.find([[class="name"[^>]*>([^<]+)</a>]], block)[2]
      if slug and title and not seen[slug] then
        seen[slug] = true
        local cover = ""
        local series = get_json(BASE .. "/series/" .. host.url_encode(slug) .. ".json")
        if series and series.cover then
          cover = absolute(series.cover)
        end
        items[#items + 1] = {
          id = slug,
          name = host.text.unescape(trim(title)),
          cover = cover,
        }
      end
    end
  end
  return { ok = true, data = { items = items, total = #items } }
end
