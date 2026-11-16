-- MangaHome (www.mangahome.com): MangaHere-mirror SSR; plain <img> reader
-- pages, "Apr 05,2024" chapter dates. Port of goIsekai
-- examples/plugins/lua/mangahome; native string patterns kept throughout (the
-- source used no re2 either), envelope { ok, data | error }, canonical fields
-- per the mangadex plugin. The site serves a JS shell to non-browser agents,
-- so every content fetch carries the Chrome UA below.
local BASE = "https://www.mangahome.com" -- manifest site_url, kept literal

local genres = require("genres")

local BROWSER_UA =
  "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36"

-- request -> body or nil (200-only); the UA is load-bearing, not cosmetic
local function http_body(url)
  local res, err = host.http.fetch(url, { headers = { ["User-Agent"] = BROWSER_UA } })
  if err ~= nil then return nil end
  if res.status ~= 200 then return nil end
  return res.body
end

-- "Apr 05,2024" -> unix midnight UTC; unparseable -> 0 (no os in sandbox)
local MONTHS = { Jan = 1, Feb = 2, Mar = 3, Apr = 4, May = 5, Jun = 6, Jul = 7, Aug = 8, Sep = 9, Oct = 10, Nov = 11, Dec = 12 }

local function date_epoch(s)
  local mon, d, y = tostring(s or ""):match("(%a%a%a)%s+(%d+)%s*,%s*(%d%d%d%d)")
  local mo = mon and MONTHS[mon]
  if not mo then return 0 end
  local yy = tonumber(y) - (mo <= 2 and 1 or 0)
  local era = math.floor(yy / 400)
  local yoe = yy - era * 400
  local doy = math.floor((153 * (mo + (mo > 2 and -3 or 9)) + 2) / 5) + tonumber(d) - 1
  local doe = yoe * 365 + math.floor(yoe / 4) - math.floor(yoe / 100) + doy
  return (era * 146097 + doe - 719468) * 86400
end

local methods = {}

-- ── search ─────────────────────────────────────────────────────────────────

methods.search = function(params)
  local q = tostring(params.title or params.query or params.q or "")
  local page = math.max(1, tonumber(params.page) or 1)
  local tags = params.tags or params.genres or {}
  if type(tags) == "string" then -- UI joins checked genres with commas
    local list = {}
    for name in tags:gmatch("[^,]+") do list[#list + 1] = name:match("^%s*(.-)%s*$") end
    tags = list
  end

  local bodies = {}
  if type(tags) == "table" and #tags > 0 then
    -- genre browse: /<slug>?name=<q>; empty query lists the whole genre
    local slug = tostring(tags[1] or "")
    slug = genres.map[slug] or slug
    local url = BASE .. "/" .. host.url_encode(slug)
    if q ~= "" then url = url .. "?name=" .. host.url_encode(q) end
    if page > 1 then url = url .. (q ~= "" and "&" or "?") .. "page=" .. tostring(page) end
    local b = http_body(url)
    if b and b ~= "" then bodies[1] = b end
  else
    -- upstream serves 12 cards/page but the UI gates the pager on a full
    -- page, so one invoke fans out over 3 upstream pages (36 rows >= 30)
    local first = (page - 1) * 3 + 1
    for p = first, first + 2 do
      local u = BASE .. "/search?name=" .. host.url_encode(q)
      if p > 1 then u = u .. "&page=" .. tostring(p) end
      local b = http_body(u)
      if b and b ~= "" then bodies[#bodies + 1] = b end
      if not b or b == "" then break end
    end
  end
  if #bodies == 0 then return { ok = false, error = "upstream_unavailable" } end

  -- cards: title attr BEFORE the href (look back), cover img after (look fwd)
  local items, seen = {}, {}
  for _, body in ipairs(bodies) do
    local pos = body:find('href="/manga/', 1, true)
    while pos do
      local slug = body:match('href="/manga/([%w_]+)"', pos)
      if slug and not seen[slug] then
        local back = body:sub(math.max(1, pos - 300), pos)
        local fwd = body:sub(pos, pos + 600)
        local title = host.text.unescape(host.text.trim(back:match('title="([^"]*)"') or ""))
        local cover = fwd:match('src="([^"]+%.jpg[^"]*)"')
        if title ~= "" and cover then
          seen[slug] = true
          if cover:sub(1, 2) == "//" then cover = "https:" .. cover end
          items[#items + 1] = { id = slug, name = title, cover = cover }
        end
      end
      pos = body:find('href="/manga/', pos + 12, true)
    end
  end
  return { ok = true, data = { items = items, total = #items } }
end

-- ── detail ─────────────────────────────────────────────────────────────────

methods.detail = function(params)
  local id = tostring(params.id or "")
  if id == "" then return { ok = true, data = {} } end
  local body = http_body(BASE .. "/manga/" .. id)
  if not body or body == "" then return { ok = false, error = "upstream_unavailable" } end

  -- "<name> Manga - Read ... For Free!" -> strip the suffixes
  local raw = body:match("<title[^>]*>([^<]*)</title>") or id
  raw = raw:gsub("Manga%s*-%s*Read.*$", "")
  raw = raw:gsub("Manga%s*$", "")
  local title = host.text.trim(host.text.unescape(raw))

  -- the manga's own cover carries class="detail-cover"; src order varies, and
  -- the "latest/recommended" cards share ocover.jpg, so try the pairing first
  local cover = body:match('src="([^"]*ocover%.jpg[^"]*)"[^>]*class="detail-cover"')
    or body:match('class="detail-cover"[^>]*src="([^"]+)"')
    or body:match('src="([^"]*ocover%.jpg[^"]*)"')
    or body:match('property="og:image" content="([^"]+)"')
    or ""
  if cover:sub(1, 2) == "//" then cover = "https:" .. cover end

  -- genre chips; the cell markup changed once already (href=/directory/slug
  -- -> bare links), keep both shapes, deduped
  local cell = body:match([[Genre%(s%):</span>%s*.-</p>]]) or ""
  local tags, seen = {}, {}
  for g in cell:gmatch('href="/directory/[^"]*"[^>]*>([^<]+)</a>') do
    local t = host.text.trim(g)
    if t ~= "" and not seen[t] then
      seen[t] = true
      tags[#tags + 1] = t
    end
  end
  for name in cell:gmatch("%s*<a[^>]*>([^<]+)</a>") do
    local t = host.text.trim(name)
    if t ~= "" and not seen[t] then
      seen[t] = true
      tags[#tags + 1] = t
    end
  end

  local status = host.text.normalize_status(body:match(">(Ongoing)<") or body:match(">(Completed)<") or "")

  local desc = host.text.trim(host.text.unescape(
    body:match('<div[^>]*class="[^"]*content[^"]*"[^>]*>(.-)</div>')
      or body:match('name="description" content="([^"]*)"')
    or ""))

  return { ok = true, data = {
    id = id,
    name = title,
    cover = cover,
    status = status,
    tags = tags,
    release_at = 0, -- the page carries no machine-readable date
    description = desc,
    authors = {}, -- the page lists no author
    url = BASE .. "/manga/" .. id,
  } }
end

-- ── chapters ───────────────────────────────────────────────────────────────
-- rows: <a href="/manga/<slug>/cNNN" ...>...<span class="time">Apr 05,2024</span>

methods.chapters = function(params)
  local id = tostring(params.id or "")
  if id == "" then return { ok = true, data = {} } end
  local body = http_body(BASE .. "/manga/" .. id)
  if not body or body == "" then return { ok = false, error = "upstream_unavailable" } end

  local out = {}
  local pos = body:find('/c%d+"', 1, false)
  while pos do
    local href = body:match('href="(/manga/' .. id .. '/c[%d.]+)"', math.max(1, pos - 200))
      or body:sub(math.max(1, pos - 200), pos + 60):match('href="(/manga/[%w_]+/c[%d.]+)"')
    local num = href and href:match("/c([%d.]+)$")
    if href and num then
      -- the date sits in a sibling cell; read up to the next chapter link
      local nxt = body:find('/c%d+"', pos + 6, false) or (pos + 1500)
      local row = body:sub(pos, nxt)
      local label = row:match('<span class="pc%-none">Ch%.? ?([^<]*)</span>')
        or row:match('name="[^"]*">.-<span[^>]*>([^<]*)</span>')
      local clean = label and host.text.trim(label) or ""
      out[#out + 1] = {
        id = id .. ":" .. num,
        number = tonumber(num) or 0,
        title = "Chapter " .. (clean ~= "" and clean or num),
        release_at = date_epoch(row:match('<span class="time">([^<]*)</span>')),
        pages = 0, -- the list carries no per-chapter page count
      }
      pos = nxt
    else
      pos = body:find('/c%d+"', pos + 4, false)
    end
  end

  if #out == 0 then return { ok = false, error = "no_chapters" } end
  return { ok = true, data = out }
end

-- ── pages ──────────────────────────────────────────────────────────────────
-- chapter id "<slug>:<num>"; the reader page embeds /store/manga/ jpgs directly

methods.pages = function(params)
  local composite = tostring(params.id or "")
  local slug, num = composite:match("^(.-):c?([%d.]+)$")
  if not slug or slug == "" or not num or num == "" then return { ok = true, data = {} } end

  local body = http_body(BASE .. "/manga/" .. slug .. "/c" .. num)
  if not body or body == "" then return { ok = false, error = "upstream_unavailable" } end

  local out = {}
  for raw in body:gmatch('src="([^"]+%.jpg[^"]*)"') do
    local u = raw
    if u:sub(1, 2) == "//" then u = "https:" .. u end
    if u:find("/store/manga/", 1, true) then out[#out + 1] = u end -- skips avatars
  end
  if #out == 0 then return { ok = false, error = "no_pages" } end
  return { ok = true, data = out }
end

-- ── genres ─────────────────────────────────────────────────────────────────

methods.genres = function()
  return { ok = true, data = genres.names }
end

function handle(req)
  local fn = methods[req.method]
  if fn == nil then return { ok = false, error = "unknown method " .. tostring(req.method) } end
  return fn(req.params or {})
end
