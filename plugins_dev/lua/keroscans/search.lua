-- Search / browse. With a query: GET /search?q=&page=N. Genre-only browse
-- (chip, no text) goes to /series?genre[i]=&page=N — /search drops an empty q.
-- Site serves 24 cards/page; the host asks for a (page, limit) slice, so the
-- merged site pages get sliced here (pola 1manga) and `pages` is reported.
local CARD = [[(?s)<article[^>]*data-series-id[^>]*>(.*?)</article>]]
local SITE_PAGE = 24
local genres = require("genres")

local function clean(s)
  return host.text.trim((host.text.unescape(s or ""):gsub("%s+", " ")))
end

local function total_of(body)
  local n = body:match("([%d,]+)%s+results?%f[%A]") or body:match("([%d,]+)%s+series%f[%A]")
  return n and tonumber((n:gsub(",", ""))) or nil
end

local function cards(ctx, body, out, seen)
  for _, m in ipairs(host.re2.find_all(CARD, body)) do
    local block = m[2]
    local slug = block:match('href="/series/comic/([^"]+)"')
    local cover = block:match('img src="([^"]+)"') or ""
    local name = clean(block:match("<h3[^>]*>(.-)</h3>") or "")
    if slug ~= nil and name ~= "" and not seen[slug] then
      seen[slug] = true
      if cover ~= "" and cover:sub(1, 1) == "/" then
        cover = ctx.BASE .. cover
      end
      out[#out + 1] = { id = slug, name = name, cover = cover }
    end
  end
end

return function(ctx)
  return function(params)
    local q = host.text.trim(tostring(params.title or params.query or params.q or ""))
    local tags = {}
    for t in tostring(params.tags or ""):gmatch("[^,]+") do
      t = host.text.trim(t)
      if t ~= "" then
        tags[#tags + 1] = t
      end
    end
    if q == "" and #tags == 0 then
      return { ok = true, data = { items = {}, total = 0, pages = 0 } }
    end
    genres.load(ctx)
    local qs = ""
    for i, t in ipairs(tags) do
      local slug = genres.slug_of(t) or t:lower():gsub("%s+", "-")
      qs = qs .. "&genre%5B" .. (i - 1) .. "%5D=" .. host.url_encode(slug)
    end

    local limit = math.max(1, tonumber(params.limit) or 12)
    local page = math.max(1, tonumber(params.page) or 1)
    local first, last = (page - 1) * limit + 1, page * limit
    local sp1, sp2 = math.ceil(first / SITE_PAGE), math.ceil(last / SITE_PAGE)

    local out, seen, total = {}, {}, nil
    for sp = sp1, sp2 do
      local url = q ~= ""
        and (ctx.BASE .. "/search?q=" .. host.url_encode(q) .. qs .. "&page=" .. sp)
        or (ctx.BASE .. "/series?page=" .. sp .. qs)
      local body, err = ctx.get(url)
      if not body then
        if err and err:find("404", 1, true) then
          break -- past the last site page: report the empty tail, not an error
        end
        return { ok = false, error = err }
      end
      total = total or total_of(body)
      cards(ctx, body, out, seen)
      if #out == 0 and sp == sp1 then
        break -- empty first site page: no further pages have cards
      end
    end

    local start = first - (sp1 - 1) * SITE_PAGE
    local slice = {}
    for i = start, math.min(start + limit - 1, #out) do
      slice[#slice + 1] = out[i]
    end
    if total == nil then
      total = (sp2 - sp1 + 1) * SITE_PAGE -- no count marker: approximate
    end
    host.log.info("keroscans: search " .. #slice .. "/" .. total .. " q=" .. q .. " tags=" .. #tags .. " p" .. page)
    return { ok = true, data = { items = slice, total = total, pages = math.ceil(total / limit) } }
  end
end
