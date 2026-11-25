local c = require("common")
local BASE = c.BASE
local trim = c.trim
local unescape = c.unescape
local strip_html = c.strip_html
local normalize_status = c.normalize_status
local http_req = c.http_req
local chapter_num = c.chapter_num
local find_all = c.find_all
local find1 = c.find1
return function(params)
  local q = params.title or params.query or params.q or ""
  local page = math.max(1, tonumber(params.page) or 1)
  local genres = params.tags or params.genres or {}
  if type(genres) == "string" then
    local list = {}
    for name in genres:gmatch("[^,]+") do
      list[#list + 1] = trim(name)
    end
    genres = list
  end

  local url
  if type(genres) == "table" and #genres > 0 then
    -- Madara genre archives live under /manga-genre/<slug>/; ?page=N is not
    -- supported by this theme, so page 2+ uses /page/N/.
    url = BASE .. "/manga-genre/" .. host.url_encode(genres[1]) .. "/"
    if page > 1 then
      url = url .. "page/" .. tostring(page) .. "/"
    end
  else
    -- Madara GET search: admin-ajax.php answers 0 bytes, use server-rendered GET.
    url = BASE .. "/?s=" .. host.url_encode(q) .. "&post_type=wp-manga"
  end

  local body, err = http_req(url)
  if err ~= nil then
    return { ok = false, error = err }
  end

  local items, seen = {}, {}
  -- one card per c-tabs-item__content block: cover <img> first, then post-title
  -- link. Capture up to the post-title close so the cover stays in the same card.
  for _, m in ipairs(find_all(body, [[(?s)<div[^>]*class="[^"]*c-tabs-item__content.*?post-title.*?</h3>]])) do
    local card = m[1]
    local href = find1(card, [[href="([^"]*/manga/[^"/]+/)"]])
    local title = find1(card, [[<h3[^>]*><a[^>]*>([^<]+)</a>]])
    if href then
      local slug = find1(href, [[/manga/([^/]+)/]]) or find1(href, [[/manga/([^/]+)$]])
      if slug and not seen[slug] then
        seen[slug] = true
        local cover = find1(card, [[data-src="[\t\n\s]*(https?://[^"]+)"]]) or find1(card, [[\ssrc="(https?://[^"]+)"]])
        items[#items + 1] = { id = slug, name = unescape(title or ""), cover = trim(cover or "") }
      end
    end
  end
  return { ok = true, data = { items = items, total = #items } }
end
