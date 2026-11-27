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

-- mdx_search: the only machine search endpoint the reader theme exposes
-- (search.js); empty q answers {success,items={}} like an empty result.
local function ajax_search(q)
  local body, err = http_req(AJAX .. "?action=mdx_search&q=" .. host.url_encode(q))
  if err ~= nil then
    return { ok = false, error = err }
  end
  local ok, data = pcall(json.decode, body)
  local payload = ok and type(data) == "table" and (data.data or data) or nil
  if type(payload) ~= "table" or type(payload.items) ~= "table" then
    return { ok = true, data = { items = {}, total = 0 } }
  end
  local items = {}
  for _, r in ipairs(payload.items) do
    local slug = type(r.url) == "string" and r.url:match("/title/([^/]+)/?$") or nil
    if slug then
      items[#items + 1] = { id = slug, name = host.text.unescape(r.title or ""), cover = r.cover or "" }
    end
  end
  return { ok = true, data = { items = items, total = #items } }
end
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
    -- the reader theme answers search over admin-ajax (action=mdx_search, JSON
    -- {items=[{title,url,cover,...}]}); the page HTML renders no result cards.
    return ajax_search(q)
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
