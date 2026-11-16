-- 1Manga.co plugin (https://1manga.co): HTML scrape, api 1.
-- The site's public GraphQL (api.mghcdn.com/graphql/mn03) is locked to its own
-- client since the 2025 rewrite: every key answers 403 "app does not have
-- permission". What it does serve is fully server-rendered HTML -- search
-- results, detail metadata, the whole chapter list and every page image
-- (imgx.mghcdn.com, plain GET, no referer, no crypto) -- so this port scrapes
-- that. Class names are hashed per deploy, so selectors stay structural
-- (bootstrap classes, labels, anchors) and never key on _3SlhO-style names.
local BASE = host.plugin.param("site_url") or "https://1manga.co"
local UA = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36"

local function get(url)
  local res, err = host.http.fetch(url, { headers = { ["User-Agent"] = UA }, timeout_ms = 30000 })
  if err ~= nil then return nil, err end
  if res.status ~= 200 then return nil, "1manga: upstream status " .. tostring(res.status) end
  return res.body, nil
end

-- React leaves <!-- --> between interpolated fragments; drop them so the
-- remaining text can be matched with plain Lua patterns.
local function decomment(s)
  return (tostring(s or ""):gsub("<!%-%-.-%-%->", ""))
end

local function unesc(s)
  return host.text.unescape(host.text.trim(tostring(s or "")))
end

local function strip_tags(s)
  return unesc((tostring(s or ""):gsub("<[^>]->", "")))
end

-- "12-08-2025" (M-D-Y, the site's chapter date) -> "2025-12-08".
local function iso_date(raw)
  local mo, d, y = string.match(tostring(raw or ""), "^(%d%d?)-(%d%d?)-(%d%d%d%d)$")
  if not mo then return "" end
  return string.format("%04d-%02d-%02d", tonumber(y), tonumber(mo), tonumber(d))
end

-- Site chapter labels include scrape noise: "CHapter 3860" and "3854-eng-li".
-- Drop those (the host renders the number itself); keep real titles verbatim.
local function clean_title(s)
  if s == "" then return "" end
  if string.match(s, "^%a+apter%s*[%d%.]+$") then return "" end
  if string.match(s, "^[%d%.]+%-%a+%-%a+$") then return "" end
  return s
end

-- longest <p> body on the page that carries prose (detail synopsis); the genre
-- row is a <p> of anchors and loses on length plus the <a> test.
local function longest_paragraph(html)
  local best = ""
  for body in string.gmatch(html or "", "<p[^>]->(.-)</p>") do
    if not body:find("<a ", 1, true) then
      local text = strip_tags(body)
      if #text > #best then best = text end
    end
  end
  return (#best >= 60) and best or ""
end

local methods = {}

-- Name -> site slug from the last genres fetch; the UI only sends names back.
local genre_slug = {}

-- ── search ─────────────────────────────────────────────────────────────────

methods.search = function(params)
  local q = tostring(params.title or params.query or params.q or "")
  local tags = params.tags or params.genres
  if type(tags) == "string" then tags = { tags } end
  if type(tags) ~= "table" then tags = {} end
  local first = tags[1]
  if type(first) == "table" then first = first.slug or first.name end

  -- A genre chip maps to the site's /genre/<lower-hyphen-name> listing; the text
  -- query keeps working on top of it via ?q=. No chip and no query => empty.
  local base
  if tostring(first or "") ~= "" then
    local label = tostring(first)
    base = BASE .. "/genre/" .. host.url_encode(genre_slug[label] or (label:lower():gsub("%s+", "-")))
  elseif q ~= "" then
    base = BASE .. "/search"
  else
    return { ok = true, data = { items = {}, total = 0 } }
  end
  local qs = (q ~= "") and ("?q=" .. host.url_encode(q)) or ""
  local urls = { base .. qs, base .. "/page/2" .. qs }

  -- Card: head (cover) + slug + title + the status tail, up to the genre <p>.
  -- RE2 syntax, not Lua patterns (`%` is a literal there). The title anchor
  -- sometimes carries rel="noindex nofollow", so attributes after href are
  -- optional — requiring a bare ">" made the match run on into the next card.
  local card = [[(?s)<div class="media-manga media">(.*?)<h4 class="media-heading"><a href="https://1manga\.co/manga/([^"]+)"[^>]*>([^<]*)</a>(.*?)<p class="_12-Zw">]]
  local out, seen = {}, {}
  for _, url in ipairs(urls) do
    local body = get(url)
    if not body then break end
    local added = 0
    for _, m in ipairs(host.re2.find_all(card, body)) do
      local head, slug, title, tail = decomment(m[2]), m[3], m[4], decomment(m[5])
      if not seen[slug] then
        seen[slug] = true
        out[#out + 1] = {
          id = slug,
          name = unesc(title),
          cover = string.match(head, 'src="(https://thumb%.mghcdn%.com/[^"]+)"') or "",
          status = host.text.normalize_status(string.match(tail, "chapters published %(([%a%-]+)%)") or ""),
        }
        added = added + 1
      end
    end
    if added == 0 then break end
  end
  -- The genre listing ignores ?q=, so a chip plus a typed query narrows here.
  if q ~= "" and tostring(first or "") ~= "" then
    local needle = q:lower()
    local kept = {}
    for _, it in ipairs(out) do
      if it.name:lower():find(needle, 1, true) then kept[#kept + 1] = it end
    end
    out = kept
  end
  host.log.info("1manga: search found " .. tostring(#out) .. " for " .. q .. " tags=" .. #tags)
  return { ok = true, data = { items = out, total = #out } }
end

-- ── detail ─────────────────────────────────────────────────────────────────

methods.detail = function(params)
  local slug = params.id or ""
  local empty = { id = slug, name = "", cover = "", status = "unknown", tags = {}, release_at = 0,
    authors = {}, artists = {}, description = "", url = BASE .. "/manga/" .. slug }
  if slug == "" then return { ok = true, data = empty } end

  local body, err = get(BASE .. "/manga/" .. host.url_encode(slug))
  if not body then return { ok = false, error = err } end
  local flat = decomment(body)

  -- <h1> starts with the title, alt names follow in <small>.
  local name = string.match(flat, "<h1[^>]->%s*([^<]+)")
    or string.match(flat, 'property="og:title" content="([^"]*)"') or ""
  name = unesc(name):gsub("^Read%s+", ""):gsub("%s+Manga Online for Free$", "")

  local cover = string.match(flat, 'property="og:image" content="([^"]*)"')
    or string.match(flat, '<img src="(https://thumb%.mghcdn%.com/[^"]+)"') or ""

  local function label(key)
    return unesc(string.match(flat, ">" .. key .. "</span><span>([^<]*)<") or "")
  end
  local function split_names(raw)
    local names = {}
    for piece in string.gmatch(raw or "", "[^;]+") do
      local one = unesc(piece)
      if one ~= "" then names[#names + 1] = one end
    end
    return names
  end

  local tags, seen = {}, {}
  for tag in string.gmatch(flat, 'genre%-label">([^<]+)<') do
    tag = unesc(tag)
    if tag ~= "" and not seen[tag] then
      seen[tag] = true
      tags[#tags + 1] = tag
    end
  end

  return { ok = true, data = {
    id = slug,
    name = name,
    cover = cover,
    status = host.text.normalize_status(label("Status")),
    tags = tags,
    release_at = 0,
    authors = split_names(label("Author")),
    artists = split_names(label("Artist")),
    description = longest_paragraph(body),
    url = BASE .. "/manga/" .. slug,
  } }
end

-- ── chapters ───────────────────────────────────────────────────────────────

methods.chapters = function(params)
  local slug = params.id or ""
  if slug == "" then return { ok = true, data = {} } end

  local body, err = get(BASE .. "/manga/" .. host.url_encode(slug))
  if not body then return { ok = false, error = err } end

  -- One <li class="list-group-item"> per chapter. Each row carries TWO anchors:
  -- the real chapter link first, then a mirror anchor whose numeric tail is a
  -- junk id (chapter-2321739 rendered for chapter 3859) -- so read the first
  -- /chapter/ href in the row and take the display number from "#N".
  local row = [[(?s)class="[^"]*list-group-item[^"]*"(.*?)</li>]]
  -- RE2 here, not Lua patterns: the slug carries '-' (a Lua lazy quantifier)
  -- and the display number sits behind an HTML comment ("#<!-- -->3862").
  local want = [[href="https://1manga\.co/chapter/]] .. slug .. [[/(chapter-[0-9.]+)"]]
  local num = [[>#([0-9.]+)<]] -- after decomment, "#<!-- -->3862" collapses to "#3862"
  local out, seen = {}, {}
  for _, m in ipairs(host.re2.find_all(row, body)) do
    local li = decomment(m[2])
    local nhit = host.re2.find(num, li)
    local shown = tonumber(nhit and nhit[2])
    -- A row can list the mirror anchor first (chapter-367389 for chapter 2), so
    -- pick the href whose numeric tail matches the displayed #N; else the first.
    local chid, first
    for _, h in ipairs(host.re2.find_all(want, li)) do
      first = first or h[2]
      if shown and tonumber(string.match(h[2], "chapter%-(.+)")) == shown then
        chid = h[2]
        break
      end
    end
    chid = chid or first
    if chid and not seen[chid] then
      seen[chid] = true
      local mdd = iso_date(string.match(li, "<small[^>]*>([^<]*)</small>"))
      -- Custom chapter label ("- One Year Promise"); the default label
      -- ("- Chapter 3862") is dropped so the UI renders just the number.
      local label = clean_title(unesc(string.match(li, '<span class="_2IG5P">%s*%-%s*([^<]*)</span>') or ""))
      out[#out + 1] = {
        id = slug .. ":" .. chid,
        number = shown or tonumber(string.match(chid, "chapter%-(.+)")) or 0,
        title = label,
        release_at = (mdd ~= "") and mdd or 0,
        pages = 0,
      }
    end
  end
  if #out == 0 then
    host.log.error("1manga: no chapter rows for " .. slug)
    return { ok = false, error = "1manga: no chapters for " .. slug }
  end
  host.log.info("1manga: found " .. tostring(#out) .. " chapters for " .. slug)
  return { ok = true, data = out } -- newest-first; the host sorts by number
end

-- ── pages ──────────────────────────────────────────────────────────────────

methods.pages = function(params)
  local composite = params.id or ""
  local slug, chid = string.match(composite, "^(.+):(chapter%-[%d%.]+)$")
  if not slug then
    return { ok = false, error = "1manga: chapter id must be slug:chapter-N, got " .. tostring(composite) }
  end

  local body, err = get(BASE .. "/chapter/" .. host.url_encode(slug) .. "/" .. chid)
  if not body then return { ok = false, error = err } end

  -- Every page img is inlined as https://imgx.mghcdn.com/<slug>/<num>/<i>.<ext>;
  -- the <i> tail gives the order (preload block and reader body both list them).
  local chnum = string.match(chid, "chapter%-(.+)$")
  local pat = [[https://imgx\.mghcdn\.com/]] .. slug .. "/" .. chnum .. [[/([0-9]+)\.[a-z]+]]
  local by_index, order = {}, {}
  for _, m in ipairs(host.re2.find_all(pat, body)) do
    local idx = tonumber(m[2])
    if idx and not by_index[idx] then
      by_index[idx] = m[1]
      order[#order + 1] = idx
    end
  end
  table.sort(order)
  local out = {}
  for _, idx in ipairs(order) do out[#out + 1] = by_index[idx] end
  if #out == 0 then
    host.log.error("1manga: no page images in " .. composite)
    return { ok = false, error = "1manga: no pages for " .. composite }
  end
  host.log.info("1manga: found " .. tostring(#out) .. " pages for " .. composite)
  return { ok = true, data = out }
end

-- ── genres ─────────────────────────────────────────────────────────────────

methods.genres = function()
  local body = get(BASE .. "/search")
  if not body then return { ok = true, data = {} } end
  local names, seen = {}, {}
  for slug, name in string.gmatch(decomment(body), 'genre/([%w%-]+)"[^>]*>([^<]+)<') do
    local label = unesc(name)
    genre_slug[label] = slug
    if not seen[label] then
      seen[label] = true
      names[#names + 1] = label
    end
  end
  host.log.info("1manga: found " .. tostring(#names) .. " genres")
  return { ok = true, data = names }
end

function handle(req)
  local fn = methods[req.method]
  if fn == nil then return { ok = false, error = "unknown method " .. tostring(req.method) } end
  return fn(req.params or {})
end
