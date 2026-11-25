local c = require("common")
local BASE = c.BASE
local GENRE_PATH = c.GENRE_PATH
local http_req = c.http_req
local find1 = c.find1
return function(params)
  local q = params.title or params.query or params.q or ""
  local page = math.max(1, tonumber(params.page) or 1)
  local genres = params.tags or params.genres or {}
  if type(genres) == "string" then
    local list = {}
    for name in genres:gmatch("[^,]+") do list[#list + 1] = host.text.trim(name) end
    genres = list
  end

  local enc_q = host.url_encode(q)
  local genre_slug
  if type(genres) == "table" and #genres > 0 then genre_slug = require("genres").map[genres[1]] or genres[1] end

  -- Madara page 2+ is /page/N/ (?page=N is not supported) and admin-ajax.php
  -- answers 0 bytes, so both branches use the server-rendered GET.
  local function mkurl(p)
    if genre_slug then
      local u = BASE .. GENRE_PATH .. host.url_encode(genre_slug) .. "/"
      if p > 1 then return u .. "page/" .. tostring(p) .. "/" end
      return u
    end
    if p > 1 then return BASE .. "/page/" .. tostring(p) .. "/?s=" .. enc_q .. "&post_type=wp-manga" end
    return BASE .. "/?s=" .. enc_q .. "&post_type=wp-manga"
  end

  -- Site-wide count from the SERP heading ("170 results for \"dungeon\"");
  -- the 12 cards on this page are not the total.
  local function parse_total(b)
    local raw = (find1(b, [[([0-9,]+)\s+results?\s+for]]) or ""):gsub(",", "")
    return tonumber(raw)
  end

  local body, err = http_req(mkurl(page))
  if err ~= nil then
    -- The site overcounts its own pager ("170 results" but /page/15/ is 404):
    -- serve an empty tail page carrying page 1's count so prev still works.
    if page > 1 and err:find("404", 1, true) then
      return { ok = true, data = { items = {}, total = parse_total(http_req(mkurl(1)) or "") or page * 12 } }
    end
    return { ok = false, error = err }
  end

  local items, seen = {}, {}
  -- one card per c-tabs-item__content block: cover <img> first, then post-title
  -- link. Capture up to the post-title close so the cover stays in the same card.
  for _, m in ipairs(host.re2.find_all([[(?s)<div[^>]*class="[^"]*c-tabs-item__content.*?post-title.*?</h3>]], body)) do
    local card = m[1]
    local href = find1(card, [[href="([^"]*/manga/[^"/]+/)"]])
    local title = find1(card, [[<h3[^>]*><a[^>]*>([^<]+)</a>]])
    if href then
      local slug = find1(href, [[/manga/([^/]+)/]]) or find1(href, [[/manga/([^/]+)$]])
      if slug and not seen[slug] then
        seen[slug] = true
        local cover = find1(card, [[data-src="[\t\n\s]*(https?://[^"]+)"]])
          or find1(card, [[\ssrc="(https?://[^"]+)"]])
        items[#items + 1] = { id = slug, name = host.text.unescape(title or ""), cover = host.text.trim(cover or "") }
      end
    end
  end
  return { ok = true, data = { items = items, total = parse_total(body) or #items } }
end
