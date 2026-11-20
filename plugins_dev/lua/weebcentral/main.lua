-- weebcentral.com — server-rendered (HTMX partials), no JSON API, no JS gate.
-- Endpoints consumed here (all plain HTML, offset-paginated search):
--   /search/data?text=q&limit=32&offset=N   HTMX result cards (one shot)
--   /search?included_tag=<Tag>              tag filtering (repeated params ok)
--   /series/<ULID>                          series page (301 -> /series/<ULID>/<Slug>)
--   /series/<ULID>/full-chapter-list        ALL chapter rows (HTMX partial)
--   /chapters/<ULID>/images?reading_style=long_strip&current_page=1   direct CDN <img>s
-- Stable id = ULID (series and chapter ULIDs both). Slugs are cosmetic.
local BASE = "https://weebcentral.com"
local UA = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36"
local LIMIT = 32

local STATUS = { complete = "completed", ongoing = "ongoing", hiatus = "hiatus", cancelled = "dropped", dropped = "dropped", upcoming = "upcoming" }

local function get(path)
  local res, err = host.http.fetch(BASE .. path, { headers = { ["User-Agent"] = UA, ["Accept"] = "text/html,*/*" }, timeout_ms = 30000 })
  if err ~= nil then return nil, err end
  if res.status ~= 200 then return nil, "weebcentral: upstream status " .. tostring(res.status) end
  return res.body, nil
end

-- Card = one top-level '<article class="bg-base-300' block; the desktop anchor
-- inside it carries ULID + slug + title, Year/Status/Tags live in the trailing
-- section of the same block. Scope = [previous sentinel, next sentinel).
-- site HTML carries entities like &#39; / &amp; / &#x27; — decode once so they
-- never reach the UI as literal text (the digits in &#39; read as "a number").
local function txt(s)
  return host.text.unescape(host.text.trim(s or ""))
end

local function cards(body)
  local out = {}
  if type(body) ~= "string" then return out end
  -- sentinel offsets for every top-level card
  local sentinels, s = {}, 1
  while true do
    local n = body:find('<article class="bg-base-300', s, true)
    if not n then break end
    sentinels[#sentinels + 1] = n
    s = n + 1
  end
  sentinels[#sentinels + 1] = #body + 1
  for i = 1, #sentinels - 1 do
    local scope = body:sub(sentinels[i], sentinels[i + 1] - 1)
    local ulid, slug, title = scope:match('([A-Z0-9]+)/([^"]*)" class="line%-clamp%-1 link link%-hover">([^<]+)</a>')
    if ulid and #ulid == 26 then
      local year = scope:match("<strong>Year:</strong>%s*<span>(%d+)</span>")
      local status = scope:match("<strong>Status:</strong>%s*<span>([^<]+)</span>")
      local authors = {}
      for name in scope:gmatch('search%?author=[^"]*"[^>]*>([^<]+)</a>') do authors[#authors + 1] = txt(name) end
      local tags = {}
      local tblock = scope:match("<strong>Tag%(s%):%s*</strong>(.-)</div>")
      if tblock then
        for t in tblock:gmatch("<span>([^<]+)</span>") do
          local tag = txt(t:gsub(",%s*$", ""))
          if tag ~= "" then tags[#tags + 1] = tag end
        end
      end
      out[#out + 1] = {
        id = ulid,
        name = txt(title),
        cover = "https://temp.compsci88.com/cover/fallback/" .. ulid .. ".jpg",
        status = STATUS[string.lower(status or "")] or "unknown",
        tags = tags,
        authors = authors,
        release_at = year and (year .. "-01-01") or 0,
        url = BASE .. "/series/" .. ulid .. "/" .. (slug or ""),
      }
    end
  end
  return out
end

local methods = {}

function methods.search(params)
  local q = params.title or params.query or params.q or ""
  local page = tonumber(params.page) or 1
  local offset = (page - 1) * LIMIT
  local tag = params.tags or params.tag
  local args = "display_mode=Full+Display&author=&text=" .. host.url_encode(q) ..
    "&sort=Best+Match&order=Descending&official=Any&anime=Any&adult=Any&limit=" .. LIMIT .. "&offset=" .. offset
  if tag and tag ~= "" then
    for t in tostring(tag):gmatch("([^,]+)") do
      args = args .. "&included_tag=" .. host.url_encode(host.text.trim(t))
    end
  end
  local body, err = get("/search/data?" .. args)
  if err then return { ok = false, error = err } end
  local items = cards(body)
  return { ok = true, data = { items = items, total = #items } }
end

function methods.detail(params)
  local id = params.id or ""
  if id == "" then return { ok = false, error = "weebcentral: missing id" } end
  local body, err = get("/series/" .. host.url_encode(id))
  if err then return { ok = false, error = err } end
  local title = body:match('<h1 class="hidden md:block text%-2xl font%-bold">(.-)</h1>') or body:match("<h1[^>]*>([^<]+)</h1>")
  local desc = body:match('<strong>Description</strong>%s*<p class="whitespace%-pre%-wrap break%-words">(.-)</p>')
  local status = body:match('<strong>Status:%s*</strong>%s*.-<a[^>]*>([^<]+)</a>') or body:match('<strong>Status:</strong>%s*.-<a[^>]*>([^<]+)</a>')
  local year = body:match('<strong>Released:%s*</strong>%s*<span>([^<]+)</span>')
  local type_ = body:match('<strong>Type:%s*</strong>%s*<a[^>]*>([^<]+)</a>')
  local authors, tags = {}, {}
  for name in body:gmatch('search%?author=[^"]*"[^>]*>([^<]+)</a>') do authors[#authors + 1] = txt(name) end
  for t in body:gmatch('included_tag=[^"]*"[^>]*>([^<]+)</a>') do tags[#tags + 1] = txt(t) end
  return { ok = true, data = {
    id = id,
    name = txt(title or id),
    cover = "https://temp.compsci88.com/cover/fallback/" .. id .. ".jpg",
    status = STATUS[string.lower(status or "")] or "unknown",
    tags = tags,
    authors = authors,
    artists = {},
    description = desc and txt(desc) or "",
    release_at = year and tonumber(year:match("%d%d%d%d")) and (year:match("%d%d%d%d") .. "-01-01") or 0,
    url = BASE .. "/series/" .. id,
  } }
end

function methods.chapters(params)
  local id = params.id or ""
  if id == "" then return { ok = true, data = {} } end
  local body, err = get("/series/" .. host.url_encode(id) .. "/full-chapter-list")
  if err then return { ok = false, error = err } end
  local out = {}
  local pos = 1
  while true do
    local a, b, ch_id, chunk = body:find('href="/chapters/([A-Z0-9]+)"[^>]*>(.-)</a>', pos)
    if not a then break end
    if #ch_id == 26 then
      local label = chunk:match('<span class="">([^<]+)</span>') or chunk:match("<span[^>]*>([^<]+)</span>")
      local dt = chunk:match('datetime="([^"]+)"')
      out[#out + 1] = {
        id = ch_id,
        number = host.text.chapter_num(label or ""),
        title = txt(label),
        release_at = dt and dt:sub(1, 10) or 0,
        created_at = dt or 0,
        pages = 0,
      }
    end
    pos = b + 1
  end
  return { ok = true, data = out }
end

function methods.pages(params)
  local id = params.id or ""
  if id == "" then return { ok = false, error = "weebcentral: missing chapter id" } end
  local body, err = get("/chapters/" .. host.url_encode(id) .. "/images")
  if err then return { ok = false, error = err } end
  local out = {}
  for src in body:gmatch('<img%s+src="(https?://[^"]+)"') do
    if not src:find("broken_image", 1, true) then out[#out + 1] = src end
  end
  if #out == 0 then return { ok = false, error = "weebcentral: no page images for " .. id } end
  return { ok = true, data = out }
end

function methods.genres(_)
  local body, err = get("/search")
  if err then return { ok = true, data = {} } end
  local seen, out = {}, {}
  for value in body:gmatch('<input type="hidden" form="advanced%-search%-form" id="tag%-[^"]*" value="([^"]+)"') do
    if not seen[value] then seen[value] = true; out[#out + 1] = value end
  end
  table.sort(out)
  return { ok = true, data = out }
end

function handle(req)
  local fn = methods[req.method]
  if fn == nil then return { ok = false, error = "unknown method " .. tostring(req.method) } end
  return fn(req.params or {})
end
