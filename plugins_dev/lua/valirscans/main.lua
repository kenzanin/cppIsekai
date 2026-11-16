-- ValirScans plugin (valirscans.org): custom Next.js reader, not WordPress.
-- Port of goIsekai examples/plugins/lua/valirscans; host.re2 argument order is
-- (pattern, subject). host.text.json_blob has no cppIsekai equivalent — ld+json
-- blocks are bracket-extracted by regex instead. The manifest "media" param is
-- unused by the source (page URLs are absolute) and is dropped here too.
-- Envelope { ok, data | error }.
local BASE = "https://valirscans.org"

-- ── helpers ────────────────────────────────────────────────────────────────

local function http_req(url)
  local res, err = host.http.fetch(url)
  if err ~= nil then return nil, err end
  if res.status ~= 200 then return nil, "valirscans: upstream status " .. tostring(res.status) end
  return res.body, nil
end

-- first capture group of the first match; nil when the pattern does not match.
local function find1(s, pattern)
  local m = host.re2.find(pattern, tostring(s or ""))
  if #m >= 2 then return m[2] end
  if #m == 1 then return m[1] end
  return nil
end

local function is_abs_url(s)
  return #host.re2.find("^https?://", tostring(s or "")) > 0
end

-- First decoded ld+json block whose "@type" is "Book", or nil. The page
-- carries several ld+json blocks; the lazy match stays inside one tag because
-- JSON-LD never contains "</script>".
local function jsonld_book(html)
  for _, m in ipairs(host.re2.find_all(
    [[(?s)<script type="application/ld\+json">(.*?)</script>]], html)) do
    local ok, obj = pcall(json.decode, m[2])
    if ok and type(obj) == "table" and obj["@type"] == "Book" then return obj end
  end
  return nil
end

-- Series status lives in the flight payload right after the series'
-- coverImage entry: read the first "status":"X" following the cover path.
-- (goIsekai bounded this to a 600-char window via find_index; first-match-after
-- is equivalent in practice and needs no second primitive.)
local function flight_status(html, cover_path)
  if not cover_path or cover_path == "" then return "" end
  local pat = '(?s)\\"coverImage\\":\\"' .. host.re2.quote(cover_path) .. '.*?\\"status\\":\\"([A-Z_]+)\\"'
  local m = host.re2.find(pat, html)
  if #m >= 2 then return m[2] end
  return ""
end

local methods = {}

-- ── search ─────────────────────────────────────────────────────────────────

methods.search = function(params)
  local q = params.title or params.query or params.q or ""
  local body, err = http_req(BASE .. "/api/search?q=" .. host.url_encode(q))
  if err ~= nil then return { ok = false, error = err } end
  local ok, data = pcall(json.decode, body)
  if not ok or type(data) ~= "table" or type(data.series) ~= "table" then
    return { ok = true, data = { items = {}, total = 0 } }
  end
  local items = {}
  for _, r in ipairs(data.series) do
    local cover = r.coverImage or ""
    if cover ~= "" and not is_abs_url(cover) then cover = BASE .. cover end
    items[#items + 1] = { id = r.urlSlug or r.slug, name = r.title or "", cover = cover }
  end
  return { ok = true, data = { items = items, total = #items } }
end

-- ── detail ─────────────────────────────────────────────────────────────────

methods.detail = function(params)
  local manga_id = params.id or ""
  local empty = { id = manga_id, name = "", cover = "", status = "unknown", tags = {}, release_at = 0,
    description = "", authors = {}, artists = {}, url = BASE .. "/series/comic/" .. manga_id }
  if manga_id == "" then return { ok = true, data = empty } end

  local html, err = http_req(BASE .. "/series/comic/" .. manga_id)
  if err ~= nil then return { ok = false, error = err } end

  local data = empty
  local book = jsonld_book(html)
  if book then
    data.name = book.name or ""
    if type(book.author) == "table" and book.author.name then
      data.authors = { book.author.name }
    elseif type(book.author) == "string" and book.author ~= "" then
      data.authors = { book.author }
    end
    data.description = book.description or ""
    local img = book.image or ""
    if img ~= "" then
      data.cover = is_abs_url(img) and img or BASE .. img
      -- status: anchor on the cover path (relative) in the flight payload
      local status = flight_status(html, img)
      if status ~= "" then data.status = host.text.normalize_status(status) end
    end
    if type(book.genre) == "table" then
      local gs = {}
      for _, g in ipairs(book.genre) do gs[#gs + 1] = g end
      data.tags = gs
    end
  end

  -- Fallbacks for anything JSON-LD missed (or if the block was absent)
  if data.name == "" then
    data.name = host.text.unescape(find1(html, [[<meta property="og:title" content="([^"]*)"]]) or "")
  end
  if data.cover == "" then
    data.cover = find1(html, [[<meta property="og:image" content="([^"]*)"]]) or ""
  end
  if data.status == "unknown" then
    -- retry the flight payload with whatever origin the cover URL carries
    local origin = find1(data.cover, [[(https?://[^/]+)?(/uploads/series/[^"]+)]]) or data.cover
    local st = flight_status(html, origin)
    if st ~= "" then data.status = host.text.normalize_status(st) end
  end
  return { ok = true, data = data }
end

-- ── chapters ───────────────────────────────────────────────────────────────

methods.chapters = function(params)
  local manga_id = params.id or ""
  if manga_id == "" then return { ok = true, data = {} } end
  local html, err = http_req(BASE .. "/series/comic/" .. manga_id)
  if err ~= nil then return { ok = false, error = err } end

  -- Chapter rows are <a href="/series/comic/{slug}/chapter/{N}"> where N may
  -- be decimal (7.1); keep the raw token so URLs round-trip exactly. The same
  -- URL repeats for the "latest chapter" card, so dedupe by token.
  local pat = '/series/comic/' .. host.re2.quote(manga_id) .. [[/chapter/([\d.]+)]]
  local nums, seen = {}, {}
  for _, m in ipairs(host.re2.find_all(pat, html)) do
    local raw = m[2]
    if raw and raw ~= "" and not seen[raw] then
      seen[raw] = true
      nums[#nums + 1] = raw
    end
  end
  -- newest-first (descending number) per ABI convention
  table.sort(nums, function(a, b) return (tonumber(a) or 0) > (tonumber(b) or 0) end)

  local out = {}
  for _, raw in ipairs(nums) do
    out[#out + 1] = {
      id = manga_id .. ":" .. raw,
      number = tonumber(raw) or 0,
      title = "Chapter " .. raw,
      release_at = 0, -- list page carries no per-chapter dates
      pages = 0,
    }
  end
  return { ok = true, data = out }
end

-- ── pages ──────────────────────────────────────────────────────────────────

methods.pages = function(params)
  local composite = params.id or ""
  local manga_id, num = composite:match("^(.-):([%d.]+)$")
  if not manga_id or not num or manga_id == "" then
    return { ok = false, error = "valirscans: chapter id must be slug:N, got " .. composite }
  end
  local html, err = http_req(BASE .. "/series/comic/" .. manga_id .. "/chapter/" .. num)
  if err ~= nil then return { ok = false, error = err } end

  -- Page images are direct media.valirscans.org URLs in reading order; dedupe
  -- keeps order (a chapter cover can repeat via srcset).
  local pat = [[(https://media\.valirscans\.org/series/]]
    .. host.re2.quote(manga_id)
    .. [[/\d+/p-[^"\\]+\.webp)]]
  local out, seen = {}, {}
  for _, m in ipairs(host.re2.find_all(pat, html)) do
    if m[1] ~= "" and not seen[m[1]] then
      seen[m[1]] = true
      out[#out + 1] = m[1]
    end
  end
  if #out == 0 then return { ok = false, error = "valirscans: no pages for " .. composite } end
  return { ok = true, data = out }
end

function handle(req)
  local fn = methods[req.method]
  if fn == nil then return { ok = false, error = "unknown method " .. tostring(req.method) } end
  return fn(req.params or {})
end
