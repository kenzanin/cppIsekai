local c = require("common")
local BASE = c.BASE
local http_req = c.http_req
local genre_map = c.genre_map
local find1 = c.find1
return function(params)
  local q = params.title or params.query or params.q or ""
  local page = math.max(1, tonumber(params.page) or 1)
  local genres = params.tags or params.genres or {}
  if type(genres) == "string" then -- UI joins checked genres with commas
    local list = {}
    for name in genres:gmatch("[^,]+") do
      list[#list + 1] = name:match("^%s*(.-)%s*$")
    end
    genres = list
  end

  local url
  if type(genres) == "table" and #genres > 0 then
    -- Madara genre archives: /manga-genre/<slug>/, page N as /page/N/. Chip
    -- labels map to slugs via the shared genre_map; unknown names degrade to
    -- the lower-hyphen guess.
    local key = genres[1]
    local slug = (genre_map and genre_map[key]) or key:lower():gsub("%s+", "-")
    url = BASE .. "/manga-genre/" .. host.url_encode(slug) .. "/"
    if page > 1 then
      url = url .. "page/" .. tostring(page) .. "/"
    end
  else
    -- Madara GET search: admin-ajax.php answers 0 bytes, use server-rendered GET.
    url = BASE .. "/?s=" .. host.url_encode(q) .. "&post_type=wp-manga"
  end

  -- CF challenge zone: render via the shared browser fetch (goto+outerHTML).
  local body, err = http_req(url)
  if err ~= nil then
    return { ok = false, error = err }
  end

  local items, seen = {}, {}
  -- Madara has two card shapes: search = .c-tabs-item__content, genre archives =
  -- .page-item-detail (badge variant, title in h4). Parse both, dedupe by slug.
  local blocks = {}
  for _, m in ipairs(host.re2.find_all([[(?s)<div[^>]*class="[^"]*page-item-detail[^"]*".*?post-title.*?</h3>]], body)) do
    blocks[#blocks + 1] = m[1]
  end
  for _, m in ipairs(host.re2.find_all([[(?s)<div[^>]*class="[^"]*c-tabs-item__content.*?post-title.*?</h3>]], body)) do
    blocks[#blocks + 1] = m[1]
  end
  for _, card in ipairs(blocks) do
    local href = find1(card, [[href="([^"]*/manga/[^"/]+/)]])
    if href then
      local slug = find1(href, [[/manga/([^/]+)/]]) or find1(href, [[/manga/([^/]+)$]])
      if slug and not seen[slug] then
        seen[slug] = true
        local cover = find1(card, [[data-src="[\t\n\s]*(https?://[^"]+)"]]) or find1(card, [[\ssrc="(https?://[^"]+)"]])
        local title = find1(card, [[<h[1-6][^>]*>\s*<a[^>]*>([^<]+)</a>]])
        items[#items + 1] = { id = slug, name = host.text.unescape(host.text.trim(title or "")), cover = host.text.trim(cover or "") }
      end
    end
  end
  return { ok = true, data = { items = items, total = #items } }
end
