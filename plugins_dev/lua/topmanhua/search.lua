local c = require("common")
local BASE = c.BASE
local CDN_COVER = c.CDN_COVER
local XHR = c.XHR
local http_req = c.http_req
local find1 = c.find1
local search_json = c.search_json
local cover_from = c.cover_from
local lookup_author = c.lookup_author
local parse_genre_cards = c.parse_genre_cards
local genres_scrape = c.genres_scrape
return function(params)
  local q = host.text.trim(params.title or params.query or params.q or "")
  local page = math.max(1, tonumber(params.page) or 1)
  local genres = params.tags or params.genres or {}
  if type(genres) == "string" then
    local list = {}
    for name in genres:gmatch("[^,]+") do list[#list + 1] = host.text.trim(name) end
    genres = list
  end

  -- name or raw slug both accepted: resolve via the scraped nav map
  if type(genres) == "table" and #genres > 0 then
    local gslug = host.url_encode(genres_scrape().by_name[genres[1]] or genres[1])
    local gurl = BASE .. "manhua-genre/" .. gslug .. "/"
    if page > 1 then
      -- a bare /page/N/ 302s to an http:// URL that then 502s; the orderby
      -- param keeps the redirect away
      gurl = BASE .. "manhua-genre/" .. gslug .. "/page/" .. page .. "/?orderby=latest"
    end
    local body, err = http_req(gurl)
    if err ~= nil then return { ok = false, error = err } end
    local items = parse_genre_cards(body)
    return { ok = true, data = { items = items, total = #items } }
  end

  local body, err = search_json(q)
  if err ~= nil then return { ok = false, error = err } end
  local ok, decoded = pcall(json.decode, body)
  if not ok or type(decoded) ~= "table" then return { ok = true, data = { items = {}, total = 0 } } end

  -- max 6 hits; the page param is ignored by the endpoint (the host paginates).
  local items = {}
  for _, r in ipairs(decoded.data or {}) do
    local slug = r.slug
    if slug and slug ~= "" then
      items[#items + 1] = { id = slug, name = host.text.unescape(r.name or ""), cover = cover_from(r) }
    end
  end
  return { ok = true, data = { items = items, total = #items } }
end

-- ── detail ─────────────────────────────────────────────────────────────────

