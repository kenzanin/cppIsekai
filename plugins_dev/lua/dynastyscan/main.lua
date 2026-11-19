-- dynasty-scans.com (Dynasty Reader): Rails site; everything but search has a
-- JSON twin at the same path (.json suffix). Port of goIsekai
-- examples/plugins/lua/dynastyscan to the cppIsekai host ABI (host.http.fetch
-- + status check; host.re2.* pattern-first). Pages are plain URL strings.
local BASE = "https://dynasty-scans.com" -- single-domain; main.json param kept verbatim, unused

-- host.text.trim: utf8-aware, case-preserving (plugin-host-promotion).
local trim = host.text.trim

local function get_json(url)
  return (host.http.get_json(url, { headers = { accept = "application/json" } }))
end

local function absolute(path)
  if path == nil or path == "" then return "" end
  local p = tostring(path)
  if p:match("^https?://") then return p end
  return BASE .. p
end

local methods = {}

-- ── search ─────────────────────────────────────────────────────────────────
-- No /search.json: search (and genre browse) scrape HTML. Tag archives need
-- view=groupings or only /chapters links appear and the series match finds
-- nothing (source lesson, kept).
methods.search = function(params)
  local query = tostring(params.title or params.query or params.q or "")
  local page = tonumber(params.page) or 1
  local genres = type(params.tags) == "table" and params.tags
    or type(params.genres) == "table" and params.genres or {}
  local genre_mode = #genres > 0

  local url
  if genre_mode then
    local g = genres[1]
    if type(g) == "table" then g = g.slug or g.name end
    url = BASE .. "/tags/" .. host.url_encode(tostring(g)) .. "?view=groupings"
    if page > 1 then url = url .. "&page=" .. tostring(page) end
  else
    url = BASE .. "/search?q=" .. host.url_encode(query)
    if page > 1 then url = url .. "&page=" .. tostring(page) end
  end

  local res, err = host.http.fetch(url)
  if err ~= nil or res.status ~= 200 then
    return { ok = false, error = err or "dynastyscan: upstream status " .. tostring(res and res.status) }
  end
  local body = res.body or ""

  local items, seen = {}, {}
  if genre_mode then
    -- Cover cards with single-quoted attrs: <li class='span2'><a class='thumbnail'
    -- href='/series/slug'><img src=...><div class='caption'><b>TITLE</b>...
    -- The card carries the cover: 25 cards stay 1 request, no per-series fetch.
    for _, m in ipairs(host.re2.find_all([[(?s)<li class='span2'>(.*?)</li>]], body)) do
      local block = m[2]
      local slug = host.re2.find([[href='/series/([^']+)]], block)[2]
      local cover = host.re2.find([[src="([^"]+)"]], block)[2]
      local title = host.re2.find([[class='caption'><b>([^<]+)</b>]], block)[2]
      if slug and title and not seen[slug] then
        seen[slug] = true
        items[#items + 1] = {
          id = slug,
          name = host.text.unescape(trim(title)),
          cover = absolute(host.text.unescape(cover or "")),
        }
      end
    end
  else
    -- Definition lists: <dd>...<a href="/series/slug" class="name">TITLE</a>.
    -- Whole-entry match keeps title paired with the right slug when titles
    -- repeat. Search cards carry no <img> at all, so the cover comes from the
    -- series JSON — the same response detail reads (N+1 by site design).
    for _, m in ipairs(host.re2.find_all([[(?s)<dd>(.*?)</dd>]], body)) do
      local block = m[2]
      local slug = host.re2.find([[href="/series/([a-z0-9_]+)"]], block)[2]
      local title = host.re2.find([[class="name"[^>]*>([^<]+)</a>]], block)[2]
      if slug and title and not seen[slug] then
        seen[slug] = true
        local cover = ""
        local series = get_json(BASE .. "/series/" .. host.url_encode(slug) .. ".json")
        if series and series.cover then cover = absolute(series.cover) end
        items[#items + 1] = {
          id = slug,
          name = host.text.unescape(trim(title)),
          cover = cover,
        }
      end
    end
  end
  return { ok = true, data = { items = items, total = #items } }
end

-- ── detail ─────────────────────────────────────────────────────────────────
-- /series/<slug>.json also carries the chapter list ("taggings"), so detail
-- and chapters share one request shape.
methods.detail = function(params)
  local slug = tostring(params.id or "")
  local data = get_json(BASE .. "/series/" .. host.url_encode(slug) .. ".json")
  if not data then
    -- keep the id so an unreachable source still reads as a series, not {}
    return { ok = true, data = {
      id = slug, name = "", cover = "", status = "unknown", tags = {},
      release_at = 0, description = "", authors = {}, artists = {},
      url = BASE .. "/series/" .. slug,
    } }
  end

  -- Tag types: Author / General (genres) / Status. The top-level "type" is
  -- the container kind (Series/Anthology/Doujin) — not a status.
  local authors, tags, status = {}, {}, "unknown"
  for _, tag in ipairs(data.tags or {}) do
    if tag.type == "Author" then
      authors[#authors + 1] = tag.name or ""
    elseif tag.type == "General" then
      tags[#tags + 1] = tag.name or ""
    elseif tag.type == "Status" and status == "unknown" then
      status = host.text.normalize_status(tag.name or "")
    end
  end

  local description = host.text.strip_html(data.description or "")
  description = host.re2.replace([[\s+]], " ", description) -- collapse runs of whitespace
  return { ok = true, data = {
    id = slug,
    name = data.name or "",
    cover = absolute(data.cover),
    status = status,
    tags = tags,
    -- the series JSON carries no release date
    release_at = 0,
    description = description,
    authors = authors,
    artists = {},
    url = BASE .. "/series/" .. slug,
  } }
end

-- ── chapters ───────────────────────────────────────────────────────────────
methods.chapters = function(params)
  local slug = tostring(params.id or "")
  local data = get_json(BASE .. "/series/" .. host.url_encode(slug) .. ".json")

  local chapters = {}
  for _, tag in ipairs((data and data.taggings) or {}) do
    local permalink = tag.permalink
    if permalink and permalink ~= "" then
      local label = trim(tag.title or "")
      chapters[#chapters + 1] = {
        id = permalink,
        number = host.text.chapter_num(label) or 0,
        title = label,
        -- taggings carries no dates
        release_at = 0,
        pages = 0,
      }
    end
  end

  -- taggings is newest-first already; the explicit re-sort is cheap insurance
  table.sort(chapters, function(a, b)
    return (a.number or 0) > (b.number or 0)
  end)
  return { ok = true, data = chapters }
end

-- ── pages ──────────────────────────────────────────────────────────────────
methods.pages = function(params)
  local permalink = tostring(params.id or "")
  local data = get_json(BASE .. "/chapters/" .. host.url_encode(permalink) .. ".json")

  local out = {}
  for _, p in ipairs((data and data.pages) or {}) do
    local url = absolute(p.url)
    if url ~= "" then out[#out + 1] = url end
  end
  if #out == 0 then
    return { ok = false, error = "dynastyscan: no pages for " .. permalink }
  end
  return { ok = true, data = out }
end

-- ── genres ─────────────────────────────────────────────────────────────────
-- Live from /tags so the list cannot drift from what archive URLs accept;
-- tag links appear nowhere but series pages, the index is the only source.
methods.genres = function()
  local res, err = host.http.fetch(BASE .. "/tags")
  if err ~= nil or res.status ~= 200 then
    return { ok = false, error = err or "dynastyscan: tags fetch failed" }
  end
  local names, seen = {}, {}
  for _, m in ipairs(host.re2.find_all([[href="/tags/([a-z0-9_%-]+)"[^>]*>([^<]+)</]], res.body or "")) do
    local label = host.text.unescape(trim(m[3]))
    if m[2] ~= "" and label ~= "" and not seen[m[2]] then
      seen[m[2]] = true
      names[#names + 1] = label
    end
  end
  table.sort(names)
  return { ok = true, data = names }
end

function handle(req)
  local fn = methods[req.method]
  if fn == nil then return { ok = false, error = "unknown method " .. tostring(req.method) } end
  return fn(req.params or {})
end
