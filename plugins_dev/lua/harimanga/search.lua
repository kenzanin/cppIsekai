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
    -- Madara genre archives: /manga-genre/<slug>/, page N as /page/N/.
    local slug = (genre_map and genre_map[genres[1]]) or genres[1]
    url = BASE .. "/manga-genre/" .. host.url_encode(slug) .. "/"
    if page > 1 then
      url = url .. "page/" .. tostring(page) .. "/"
    end
  else
    -- Madara GET search: admin-ajax.php answers 0 bytes, use server-rendered GET.
    url = BASE .. "/?s=" .. host.url_encode(q) .. "&post_type=wp-manga"
  end

  -- CF challenge zone: plain/chrome116 curl eats 403 -> render via host browser
  -- (comix pattern; goto solves the challenge, then grab outerHTML).
  local _, gerr = host.browser.call("goto", { url = url, timeout_ms = 60000 })
  if gerr ~= nil then
    return { ok = false, error = "harimanga: goto failed: " .. gerr }
  end
  local body, eerr = host.browser.call("evaluate", { script = "document.documentElement.outerHTML", timeout_ms = 30000 })
  if eerr ~= nil or body == nil or body == "" then
    return { ok = false, error = "harimanga: search render failed: " .. tostring(eerr) }
  end

  local items, seen = {}, {}
  -- one card per c-tabs-item__content block: cover <img> first, then post-title
  -- link; capture up to the close so the cover stays in the same card.
  for _, m in ipairs(host.re2.find_all([[(?s)<div[^>]*class="[^"]*c-tabs-item__content.*?post-title.*?</h3>]], body)) do
    local card = m[1]
    local href = find1(card, [[href="([^"]*/manga/[^"/]+/)]])
    if href then
      local slug = find1(href, [[/manga/([^/]+)/]]) or find1(href, [[/manga/([^/]+)$]])
      if slug and not seen[slug] then
        seen[slug] = true
        local cover = find1(card, [[data-src="[\t\n\s]*(https?://[^"]+)"]]) or find1(card, [[\ssrc="(https?://[^"]+)"]])
        local title = find1(card, [[<h3[^>]*><a[^>]*>([^<]+)</a>]])
        items[#items + 1] = { id = slug, name = host.text.unescape(title or ""), cover = host.text.trim(cover or "") }
      end
    end
  end
  return { ok = true, data = { items = items, total = #items } }
end
