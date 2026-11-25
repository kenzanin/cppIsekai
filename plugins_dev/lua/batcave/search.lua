local c = require("common")
local BASE = c.BASE
local dle = c.dle
local site_get = c.site_get
local json_data = c.json_data
local abs_url = c.abs_url
local path_of = c.path_of
local manga_id_of = c.manga_id_of
local parse_readed_cards = c.parse_readed_cards
local genre_ids = c.genre_ids
local total_memo = c.total_memo
local pager_pages = c.pager_pages
local remember = c.remember
local load_genres = c.load_genres
return function(params)
  local query = params.title or params.query or params.q or ""
  local page = math.max(1, tonumber(params.page) or 1)
  local genres = params.tags or params.genres or {}
  if type(genres) == "string" then
    genres = { genres }
  end

  local base, key
  if type(genres) == "table" and #genres > 0 then
    -- Genre/category browse uses ComicList with g=<id>. First genre wins;
    -- the chip sends the display name, resolved via the genre map built by
    -- genres() (numeric passthrough kept for old cached URLs).
    load_genres()
    local id = tonumber(genre_ids[tostring(genres[1])] or tostring(genres[1]):match("(%d+)"))
    if not id then
      return { ok = true, data = { items = {}, total = 0, pages = 0 } }
    end
    base = "/ComicList/g=" .. id .. "/"
    key = "g:" .. id
  elseif query ~= "" then
    base = "/search/" .. host.url_encode(query)
    key = "q:" .. query
  else
    return { ok = true, data = { items = {}, total = 0, pages = 0 } }
  end

  local function page_url(p)
    if p <= 1 then
      return base
    end
    local b = base
    if b:sub(-1) ~= "/" then
      b = b .. "/"
    end
    return b .. "page/" .. p .. "/"
  end

  local body = site_get(page_url(page))
  if not body or body == "" then
    return { ok = false, error = "batcave: search fetch failed" }
  end
  local items = parse_readed_cards(body)
  local maxp = pager_pages(body)

  local total = total_memo[key]
  if total == nil then
    if maxp <= 1 then
      total = #items
    else
      local per = page < maxp and #items or #parse_readed_cards(site_get(page_url(1)))
      local last = page == maxp and #items or #parse_readed_cards(site_get(page_url(maxp)))
      total = per > 0 and (maxp - 1) * per + last or #items
    end
    total = remember(key, total)
  end
  return { ok = true, data = { items = items, total = total, pages = maxp } }
end

-- ── detail ─────────────────────────────────────────────────────────────────
-- GET /<news_id-slug>.html -> window.__DATA__ (no chapters there) + JSON-LD
-- ComicSeries as the fallback source for description/cover/genres.
