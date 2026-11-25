local c = require("common")
local BASE = c.BASE
local genres = c.genres
local BROWSER_UA = c.BROWSER_UA
local http_body = c.http_body
local date_epoch = c.date_epoch
return function(params)
  local q = tostring(params.title or params.query or params.q or "")
  local page = math.max(1, tonumber(params.page) or 1)
  local tags = params.tags or params.genres or {}
  if type(tags) == "string" then -- UI joins checked genres with commas
    local list = {}
    for name in tags:gmatch("[^,]+") do
      list[#list + 1] = name:match("^%s*(.-)%s*$")
    end
    tags = list
  end

  local bodies = {}
  if type(tags) == "table" and #tags > 0 then
    -- genre browse: /<slug>?name=<q>; empty query lists the whole genre
    local slug = tostring(tags[1] or "")
    slug = genres.map[slug] or slug
    local url = BASE .. "/" .. host.url_encode(slug)
    if q ~= "" then
      url = url .. "?name=" .. host.url_encode(q)
    end
    if page > 1 then
      url = url .. (q ~= "" and "&" or "?") .. "page=" .. tostring(page)
    end
    local b = http_body(url)
    if b and b ~= "" then
      bodies[1] = b
    end
  else
    -- upstream serves 12 cards/page but the UI gates the pager on a full
    -- page, so one invoke fans out over 3 upstream pages (36 rows >= 30)
    local first = (page - 1) * 3 + 1
    for p = first, first + 2 do
      local u = BASE .. "/search?name=" .. host.url_encode(q)
      if p > 1 then
        u = u .. "&page=" .. tostring(p)
      end
      local b = http_body(u)
      if b and b ~= "" then
        bodies[#bodies + 1] = b
      end
      if not b or b == "" then
        break
      end
    end
  end
  if #bodies == 0 then
    return { ok = false, error = "upstream_unavailable" }
  end

  -- cards: title attr BEFORE the href (look back), cover img after (look fwd)
  local items, seen = {}, {}
  for _, body in ipairs(bodies) do
    local pos = body:find('href="/manga/', 1, true)
    while pos do
      local slug = body:match('href="/manga/([%w_]+)"', pos)
      if slug and not seen[slug] then
        local back = body:sub(math.max(1, pos - 300), pos)
        local fwd = body:sub(pos, pos + 600)
        local title = host.text.unescape(host.text.trim(back:match('title="([^"]*)"') or ""))
        local cover = fwd:match('src="([^"]+%.jpg[^"]*)"')
        if title ~= "" and cover then
          seen[slug] = true
          if cover:sub(1, 2) == "//" then
            cover = "https:" .. cover
          end
          items[#items + 1] = { id = slug, name = title, cover = cover }
        end
      end
      pos = body:find('href="/manga/', pos + 12, true)
    end
  end
  return { ok = true, data = { items = items, total = #items } }
end
