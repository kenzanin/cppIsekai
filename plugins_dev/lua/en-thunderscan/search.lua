local c = require("common")
local BASE = c.BASE
local trim = c.trim
local cap = c.cap
local http_body = c.http_body
local fetch_detail_body = c.fetch_detail_body
return function(params)
  local query = tostring(params.title or params.query or params.q or "")
  local page = tonumber(params.page) or 1
  local genres = type(params.tags) == "table" and params.tags
    or type(params.genres) == "table" and params.genres or {}

  local url
  if #genres > 0 then
    local g = genres[1]
    if type(g) == "table" then g = g.slug or g.name end
    -- the host echoes the genres() name back; browse routes key on the slug
    local gslug = tostring(g):lower():gsub("%s+", "-")
    url = BASE .. "/genres/" .. host.url_encode(gslug) .. "/"
    if page > 1 then
      url = url .. "page/" .. tostring(page) .. "/"
    end
  elseif page > 1 then
    url = BASE .. "/page/" .. tostring(page) .. "/?s=" .. host.url_encode(query)
  else
    url = BASE .. "/?s=" .. host.url_encode(query)
  end

  local body = http_body(url)
  if not body then
    return { ok = false, error = "en-thunderscans: search fetch failed" }
  end

  local items, seen = {}, {}
  for _, card_m in ipairs(host.re2.find_all([[(?s)<div class="bsx".*?<div class="tt">.*?</div>]], body)) do
    local card = card_m[1]
    local href = cap([[href="([^"]+/comics/[^"/]+/)"]], card) or cap([[href="([^"]+)"]], card)
    local title = cap([[<div class="tt">\s*([^<]+)\s*</div>]], card) or cap([[title="([^"]+)"]], card)
    if href then
      local slug = cap([[/comics/([^/]+)/]], href) or cap([[/([^/]+)/?$]], href)
      if slug and not seen[slug] then
        seen[slug] = true
        local cover = cap([[data-src="[\t\n\s]*(https?://[^"]+)"]], card)
          or cap([[\ssrc="(https?://[^"]+)"]], card, 2)
        items[#items + 1] = {
          id = slug,
          name = host.text.unescape(trim(title)),
          cover = cover and trim(cover) or "",
        }
      end
    end
  end
  return { ok = true, data = { items = items, total = #items } }
end
