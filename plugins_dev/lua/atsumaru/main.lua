-- Atsumaru plugin (atsu.moe): Typesense-backed search, plain JSON APIs, CDN images.
-- Port of goIsekai examples/plugins/lua/atsumaru; see knowledge/references/goisekai-plugin-examples.
-- Envelope { ok, data | error }; canonical manga fields per the mangadex plugin.
local BASE = "https://atsu.moe"
-- Images are not served by the origin (atsu.moe/static/... 410 Gone); cdn.atsu.moe
-- serves the same paths, so relative image paths resolve against the CDN.
local CDN = "https://cdn.atsu.moe"

local HEADERS = { accept = "application/json" }

-- Site's own category slugs; the search filter consumes genreIds:=[...].
-- Adult entries left out: the search filter already caps content rating.
local GENRES = {
  { name = "Action", slug = "39" },
  { name = "Adventure", slug = "37" },
  { name = "Comedy", slug = "6" },
  { name = "Drama", slug = "31" },
  { name = "Fantasy", slug = "36" },
  { name = "Historical", slug = "45" },
  { name = "Horror", slug = "44" },
  { name = "Martial Arts", slug = "29" },
  { name = "Mystery", slug = "32" },
  { name = "Psychological", slug = "18" },
  { name = "Romance", slug = "9" },
  { name = "Sci-Fi", slug = "1" },
  { name = "Slice of Life", slug = "7" },
  { name = "Supernatural", slug = "22" },
  { name = "Thriller", slug = "19" },
  { name = "Tragedy", slug = "5" },
}
local GENRE_SLUGS = {}
for _, g in ipairs(GENRES) do GENRE_SLUGS[g.name] = g.slug end

-- Relative image paths come in three shapes ("/static/posters/x.jpg" from search,
-- "posters/x.jpg" from payloads, already-absolute URLs); only relative need CDN.
local function image_url(path)
  if path == nil or path == "" then return "" end
  if path:match("^https?://") then return path end
  if path:sub(1, 2) == "//" then return "https:" .. path end
  path = path:gsub("^/static/", ""):gsub("^static/", "")
  return CDN .. "/static/" .. path
end

local function get_json(url)
  local res, err = host.http.fetch(url, { accept = "application/json" })
  if err ~= nil then return nil, err end
  if res.status ~= 200 then return nil, "upstream status " .. tostring(res.status) end
  local ok, data = pcall(json.decode, res.body)
  if not ok or type(data) ~= "table" then return nil, "atsumaru: invalid JSON" end
  return data
end

-- allChapters createdAt is epoch milliseconds (int64) -> unix seconds.
local function epoch_ms(v)
  return math.floor((tonumber(v) or 0) / 1000)
end

local function strip_html(s)
  return (tostring(s or ""):gsub("<[^>]*>", " "):gsub("%s+", " "))
end

local methods = {}

methods.search = function(params)
  local q = params.title or params.query or params.q or ""
  local page = math.max(1, tonumber(params.page) or 1)
  local tags = params.tags or {}
  if type(tags) == "string" then
    local list = {}
    for name in tags:gmatch("[^,]+") do list[#list + 1] = name end
    tags = list
  end
  local filter = ""
  if type(tags) == "table" and #tags > 0 then
    local slugs = {}
    for _, name in ipairs(tags) do
      local slug = GENRE_SLUGS[name]
      if slug then slugs[#slugs + 1] = slug end
    end
    if #slugs > 0 then
      filter = " && genreIds:=[" .. table.concat(slugs, ",") .. "]"
    end
  end
  local url = BASE .. "/collections/manga/documents/search?q="
    .. host.url_encode(q ~= "" and q or "*")
    .. "&filter_by=" .. host.url_encode(filter)
    .. "&query_by=" .. host.url_encode("title,englishTitle,otherNames,authors")
    .. "&page=" .. tostring(page)
    .. "&per_page=20"
  local data, err = get_json(url)
  if err ~= nil then return { ok = false, error = err } end
  local items = {}
  if type(data.hits) == "table" then
    for _, hit in ipairs(data.hits) do
      local doc = hit.document
      if type(doc) == "table" and doc.id then
        items[#items + 1] = {
          id = doc.id,
          name = doc.title or doc.englishTitle or "",
          cover = image_url(doc.poster or doc.posterSmall),
        }
      end
    end
  end
  return { ok = true, data = { items = items, total = tonumber(data.found) or #items } }
end

methods.detail = function(params)
  local id = params.id or ""
  local data, err = get_json(BASE .. "/api/manga/page?id=" .. host.url_encode(id))
  if err ~= nil then return { ok = false, error = err } end
  local page = type(data) == "table" and data.mangaPage
  if type(page) ~= "table" then
    return { ok = false, error = "atsumaru: no mangaPage for " .. tostring(id) }
  end
  local poster = page.poster
  if type(poster) == "table" then poster = poster.image or poster.id end
  local authors = {}
  for _, a in ipairs(page.authors or {}) do
    if type(a) == "table" and a.name then authors[#authors + 1] = a.name end
  end
  local genres = {}
  for _, g in ipairs(page.genres or {}) do
    if type(g) == "table" and g.name then genres[#genres + 1] = g.name end
  end
  return {
    ok = true,
    data = {
      id = id,
      name = page.title or "",
      cover = image_url(poster),
      status = page.status or "",
      tags = genres,
      release_at = 0,
      description = strip_html(page.synopsis),
      authors = authors,
      artists = {},
      url = BASE .. "/manga/" .. tostring(id),
    },
  }
end

methods.chapters = function(params)
  local id = params.id or ""
  local data, err = get_json(BASE .. "/api/manga/allChapters?mangaId=" .. host.url_encode(id))
  if err ~= nil then return { ok = false, error = err } end
  local raw = type(data) == "table" and data.chapters or {}
  local out, seen = {}, {}
  for _, c in ipairs(type(raw) == "table" and raw or {}) do
    if type(c) == "table" and c.id then
      local label = c.title or ("Chapter " .. tostring(c.number or ""))
      local num = tonumber(c.number)
        or tonumber(tostring(label):match("[%d%.]+"))
        or 0
      -- host routes pages/CBZ by chapter number, so a second release of the
      -- same number is unreachable and only duplicates the list; keep first.
      if not seen[num] then
        seen[num] = true
        out[#out + 1] = {
          id = id .. ":" .. c.id, -- composite mangaId:chapterId, pages needs both
          number = num,
          title = label,
          release_at = epoch_ms(c.createdAt),
          pages = tonumber(c.pageCount) or 0, -- read_ch rollup needs page count
        }
      end
    end
  end
  -- deterministic ascending like mangadex (order[chapter]=asc)
  table.sort(out, function(a, b) return (a.number or 0) < (b.number or 0) end)
  return { ok = true, data = out }
end

methods.pages = function(params)
  local mangaID, chapterID = (params.id or ""):match("^([^:]+):(.+)$")
  if not mangaID or not chapterID then
    return { ok = false, error = "atsumaru: chapter id must be mangaId:chapterId, got "
      .. tostring(params.id) }
  end
  local data, err = get_json(BASE .. "/api/read/chapter?mangaId=" .. host.url_encode(mangaID)
    .. "&chapterId=" .. host.url_encode(chapterID))
  if err ~= nil then return { ok = false, error = err } end
  local read = type(data) == "table" and data.readChapter
  local list = type(read) == "table" and read.pages or nil
  local out = {}
  for _, p in ipairs(type(list) == "table" and list or {}) do
    local url = image_url(type(p) == "table" and p.image or nil)
    if url ~= "" then out[#out + 1] = url end
  end
  if #out == 0 then return { ok = false, error = "atsumaru: no pages" } end
  return { ok = true, data = out }
end

methods.genres = function()
  local names = {}
  for _, g in ipairs(GENRES) do names[#names + 1] = g.name end
  table.sort(names)
  return { ok = true, data = names }
end

function handle(req)
  local fn = methods[req.method]
  if fn == nil then return { ok = false, error = "unknown method " .. tostring(req.method) } end
  return fn(req.params or {})
end
