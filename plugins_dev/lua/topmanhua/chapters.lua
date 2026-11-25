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
  local slug = host.text.trim(tostring(params.id or ""))
  if slug == "" then return { ok = true, data = {} } end

  -- Chapter rows come from the ComicService JSON API; comicId lives only in
  -- the detail page's inline script.
  local body, err = http_req(BASE .. "manhua/" .. host.url_encode(slug))
  if err ~= nil then return { ok = false, error = err } end
  -- RE2 syntax, not Lua patterns: \s / \d, never %s / %d.
  local comic_id = find1(body, [[var comicId\s*=\s*(\d+)]]) or find1(body, [[id="comic_id"\s+value="(\d+)"]])
  if not comic_id then
    return { ok = false, error = "topmanhua: comicId missing on detail page for " .. slug }
  end

  local api, aerr = http_req(
    BASE .. "Comic/Services/ComicService.asmx/ProcessChapterList?comicId=" .. comic_id,
    { ["Referer"] = BASE .. "manhua/" .. slug })
  if aerr ~= nil then return { ok = false, error = aerr } end
  local ok, decoded = pcall(json.decode, api)
  if not ok or type(decoded) ~= "table" then
    return { ok = false, error = "topmanhua: malformed chapter list for " .. slug }
  end

  local out, seen = {}, {}
  for _, c in ipairs(decoded.chapters or {}) do
    -- the slugs are inconsistent (chapter-9-2 vs chapter-10.1): always take
    -- the API's last path segment, never rebuild it from the chapter name.
    local seg = c.url and find1(c.url, [[/([^/]+)/?$]])
    if seg and seg ~= "" and not seen[seg] then
      seen[seg] = true
      out[#out + 1] = {
        id = slug .. ":" .. seg,
        number = host.text.chapter_num(c.name or "") or 0,
        title = c.name or "",
        release_at = 0, -- API carries no dates
        pages = 0,
      }
    end
  end
  return { ok = true, data = out }
end

-- ── pages ──────────────────────────────────────────────────────────────────

