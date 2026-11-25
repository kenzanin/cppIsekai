local c = require("common")
local BASE = c.BASE
local AJAX = c.AJAX
local MANGA_PATH = c.MANGA_PATH
local auth_done = c.auth_done
local warm_auth = c.warm_auth
local http_req = c.http_req
local find1 = c.find1
local abs_url = c.abs_url
local manga_url = c.manga_url
local img_src = c.img_src
local genre_map = c.genre_map
return function(params)
  local q = host.text.trim(tostring(params.title or params.query or params.q or ""))
  local page = math.max(1, tonumber(params.page) or 1)
  local tags = params.tags or params.genres or {}
  if type(tags) == "string" then
    local list = {}
    for name in tags:gmatch("[^,]+") do
      list[#list + 1] = name:match("^%s*(.-)%s*$")
    end
    tags = list
  end

  local url
  if type(tags) == "table" and #tags > 0 then
    -- theme genre archive, server-rendered (NOT /manga-genre/ like siblings)
    local slug = (genre_map and genre_map[tags[1]]) or tags[1]
    url = BASE .. "/genre/" .. host.url_encode(slug) .. "/"
    if page > 1 then
      url = url .. "page/" .. tostring(page) .. "/"
    end
  else
    -- plain WordPress GET search; this theme's admin-ajax loader answers
    -- "no-posts" for query strings
    url = BASE .. "/?s=" .. host.url_encode(q) .. "&post_type=wp-manga"
  end

  local body, err = http_req(url)
  if err ~= nil then
    return { ok = false, error = err }
  end

  local items, seen = {}, {}
  -- two card layouts (search grid c-tabs-item__content, genre page
  -- page-listing-item) share the post-title block; one tolerant wrapper
  for _, m in
    ipairs(
      host.re2.find_all(
        [[(?s)<div[^>]*class="[^"]*(?:c-tabs-item__content|page-listing-item)[^"]*".*?post-title.*?</h3>]],
        body
      )
    )
  do
    local card = m[1]
    local href = find1(card, [[href="([^"]*/title/[^"/]+/)]])
    local title = find1(card, [[(?s)<h3[^>]*>.*?<a[^>]*>(.*?)</a>]])
    local slug = href and href:match("/title/([^/]+)/?$")
    if slug and slug ~= "" and not seen[slug] then
      seen[slug] = true
      local cover = find1(card, [[data-src="\s*(https?://[^"]+)]]) or find1(card, [[\ssrc="(https?://[^"]+)]])
      items[#items + 1] = {
        id = slug,
        name = host.text.unescape(host.text.strip_html(title or "")),
        cover = cover and host.text.trim(cover) or "",
        url = abs_url(href),
      }
    end
  end
  return { ok = true, data = { items = items, total = #items } }
end
