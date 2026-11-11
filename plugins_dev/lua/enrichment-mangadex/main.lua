-- MangaDex enrichment (metadata only): fetch candidates for the library detail page.
-- Envelope: { title = string, kind = "detail" | "covers" }
-- Returns { ok=true, data={...} } as defined in openspec detail-enrichment-fields.
-- No chapters, no pages, no login: public read-only MangaDex API.

local API = "https://api.mangadex.org"
local CDN = "https://uploads.mangadex.org"
local SITE = "https://mangadex.org"

local PREFERRED = { "en", "ja-ro", "ko-ro", "ja", "ko" }

local function pick(map)
  if type(map) ~= "table" then return "" end
  for _, lang in ipairs(PREFERRED) do
    local v = map[lang]
    if type(v) == "string" and v ~= "" then return v end
  end
  for _, v in pairs(map) do
    if type(v) == "string" and v ~= "" then return v end
  end
  return ""
end

local function get(url)
  local res, err = host.http.fetch(url, {
    accept = "application/json",
    headers = { ["User-Agent"] = "cppIsekai-Enrichment/1.0 (mangadex enrichment)" },
  })
  if err ~= nil then
    host.log.debug("GET " .. url .. " -> fetch error: " .. tostring(err))
    return nil
  end
  local ct = (res.headers and res.headers["content-type"]) or "?"
  host.log.debug("GET " .. url .. " -> " .. tostring(res.status) .. " len=" .. #(res.body or "") .. " ct=" .. ct)
  if not res or res.status ~= 200 then return nil end
  local ok, body = pcall(json.decode, res.body)
  if not ok or type(body) ~= "table" then return nil end
  return body
end

-- strip markdown/link soup from synopsis (simple: drop URLs, collapse space)
local function clean_text(s)
  s = tostring(s or "")
  s = s:gsub("%[([^%]]+)%]%(([^%)]+)%)", "%1")
  s = s:gsub("https?://%S+", "")
  s = s:gsub("\r\n", "\n")
  s = s:gsub("[ \t]+", " ")
  return (s:gsub("^%s+", "")):gsub("%s+$", "")
end

-- find a single candidate record by title: search ranking is not trusted, so
-- score by normalized exact/substring match on main + alt titles.
local function normalize(s)
  return tostring(s or ""):lower():gsub("[^%w%s]", " "):gsub("%s+", " "):gsub("^%s+", ""):gsub("%s+$", "")
end

local function best_record(query)
  local q = normalize(query)
  if q == "" then return nil end
  local enc = query:gsub("([^%w%-%._~])", function(c) return string.format("%%%02X", c:byte()) end)
  -- brackets/includes must be percent-encoded or the MangaDex API returns an empty collection
  local url = string.format(
    "%s/manga?limit=10&order%%5Brelevance%%5D=desc&title=%s&includes%%5B%%5D=author&includes%%5B%%5D=artist&includes%%5B%%5D=tag&includes%%5B%%5D=cover_art",
    API, enc
  )
  local body = get(url)
  if not body or type(body.data) ~= "table" then return nil end
  local best, bestScore
  for _, rec in ipairs(body.data) do
    local attrs = rec.attributes or {}
    local candidates = { pick(attrs.title) }
    for _, alt in ipairs(attrs.altTitles or {}) do candidates[#candidates + 1] = pick(alt) end
    for _, c in ipairs(candidates) do
      local n = normalize(c)
      if n ~= "" then
        local score
        if n == q then score = 100
        elseif n:find(q, 1, true) then score = 60
        elseif q:find(n, 1, true) then score = 40
        end
        if score then
          local lenPenalty = math.abs(#q - #n)
          score = score - lenPenalty * 0.05
          if bestScore == nil or score > bestScore then best, bestScore = rec, score end
        end
      end
    end
  end
  return best
end

local function extract_tags(rec)
  local out, seen = {}, {}
  for _, t in ipairs((rec.attributes and rec.attributes.tags) or {}) do
    local attrs = t.attributes or {}
    local grp = attrs.group
    if grp == "genre" or grp == "theme" or grp == "format" then
      local name = pick(attrs.name)
      if name ~= "" and not seen[name] then seen[name] = true; out[#out + 1] = name end
    end
  end
  return out
end

local function extract_names(rec, want)
  local out, seen = {}, {}
  for _, rel in ipairs(rec.relationships or {}) do
    if rel.type == want then
      local name = rel.attributes and rel.attributes.name or ""
      if name ~= "" and not seen[name] then seen[name] = true; out[#out + 1] = name end
    end
  end
  return out
end

local function cover_urls(rec)
  local id = rec.id
  if not id then return {} end
  local out, seen = {}, {}
  -- primary cover from relationships (if included inline)
  for _, rel in ipairs(rec.relationships or {}) do
    if rel.type == "cover_art" and rel.attributes and rel.attributes.fileName then
      local f = rel.attributes.fileName
      local url = string.format("%s/covers/%s/%s", CDN, id, f)
      if not seen[url] then seen[url] = true; out[#out + 1] = url end
    end
  end
  -- full cover catalogue (all editions)
  local cat = get(string.format("%s/cover?manga%%5B%%5D=%s&limit=50", API, id))
  if cat and type(cat.data) == "table" then
    -- locale preference: English, then Japanese, then unknown
    local en, ja, rest = {}, {}, {}
    for _, item in ipairs(cat.data) do
      local attrs = item.attributes or {}
      local f = attrs.fileName
      if f and f ~= "" then
        local url = string.format("%s/covers/%s/%s", CDN, id, f)
        if not seen[url] then
          seen[url] = true
          local loc = attrs.locale or ""
          if loc == "en" then en[#en + 1] = url
          elseif loc == "ja" then ja[#ja + 1] = url
          else rest[#rest + 1] = url end
        end
      end
    end
    for _, u in ipairs(en) do out[#out + 1] = u end
    for _, u in ipairs(ja) do out[#out + 1] = u end
    for _, u in ipairs(rest) do out[#out + 1] = u end
  end
  -- cap at 5 candidates: primary cover first, then locale-preferred catalogue
  while #out > 5 do out[#out] = nil end
  return out
end

-- recommendations: reuse the source plugin's approach on the matched record:
-- MangaDex recommendation endpoint gives scored pairs; the OTHER manga is the candidate.
local function build_recommendations(rec)
  local id = rec.id or ""
  local body, err = get(string.format("%s/manga/%s/recommendation", API, id))
  if err ~= nil or not body or type(body.data) ~= "table" then
    return { ok = false, error = err or "recommendation failed" }
  end
  local scored = {}
  for _, entry in ipairs(body.data) do
    local score = tonumber((entry.attributes or {}).score) or 0
    for _, rel in ipairs(entry.relationships or {}) do
      if rel.type == "manga" and rel.id and rel.id ~= id then
        scored[#scored + 1] = { id = rel.id, score = score }
        break
      end
    end
  end
  table.sort(scored, function(a, b) return a.score > b.score end)
  while #scored > 10 do scored[#scored] = nil end
  if #scored == 0 then return { ok = true, data = { items = {} } } end
  local ids = {}
  for _, s in ipairs(scored) do ids[#ids + 1] = s.id end
  local ids_q = table.concat(ids, "&ids[]=")
  local bbody = get(string.format("%s/manga?limit=10&ids[]=%s&includes[]=cover_art", API, ids_q))
  local by_id = {}
  if bbody and type(bbody.data) == "table" then
    for _, m in ipairs(bbody.data) do by_id[m.id] = m end
  end
  local items = {}
  for _, s in ipairs(scored) do
    local m = by_id[s.id]
    if m then
      local name = pick((m.attributes or {}).title)
      if name ~= "" then
        items[#items + 1] = { id = m.id, name = name, url = SITE .. "/title/" .. m.id }
      end
    end
  end
  return { ok = true, data = { items = items } }
end

-- main title + localized alTitles, one {title, lang} entry each; no de-dupe here (host merges)
local function alt_titles(rec)
  local out = {}
  local attrs = rec.attributes or {}
  local main = attrs.title or {}
  if type(main) == "table" then
    for lang, v in pairs(main) do
      if type(v) == "string" and v ~= "" then out[#out + 1] = { title = v, lang = tostring(lang), main = true } end
    end
  end
  for _, m in ipairs(attrs.altTitles or {}) do
    for lang, v in pairs(m) do
      if type(v) == "string" and v ~= "" then out[#out + 1] = { title = v, lang = tostring(lang), main = false } end
    end
  end
  return out
end

local function build_detail(rec)
  local attrs = rec.attributes or {}
  return {
    title = pick(attrs.title),
    synopsis = clean_text(pick(attrs.description)),
    status = attrs.status,
    year = attrs.year,
    authors = extract_names(rec, "author"),
    artists = extract_names(rec, "artist"),
    tags = extract_tags(rec),
    alt_titles = alt_titles(rec),
    source_url = SITE .. "/title/" .. tostring(rec.id or ""),
  }
end

local function build_covers(rec)
  return {
    source_url = SITE .. "/title/" .. tostring(rec.id or ""),
    covers = cover_urls(rec),
  }
end

function handle(req)
  local p = req.params or {}
  local title = tostring(p.title or "")
  local kind = tostring(p.kind or "detail")
  if title == "" then return { ok = false, error = "missing title" } end
  if kind ~= "detail" and kind ~= "covers" and kind ~= "recommendations" then return { ok = false, error = "kind must be detail|covers|recommendations" } end
  if #title > 512 then return { ok = false, error = "title too long" } end

  local rec = best_record(title)
  if not rec then return { ok = false, error = "no match found" } end

  if kind == "recommendations" then
    return build_recommendations(rec)
  end
  local data = kind == "covers" and build_covers(rec) or build_detail(rec)
  return { ok = true, data = data }
end
