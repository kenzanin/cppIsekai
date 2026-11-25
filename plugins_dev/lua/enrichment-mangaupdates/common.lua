-- MangaUpdates enrichment (metadata only): fetch candidates for the library detail page.
-- Envelope: { title = string, kind = "detail" | "covers" }
-- Returns { ok=true, data={...} } as defined in openspec detail-enrichment-fields.
-- No chapters, no pages, no login: public MangaUpdates v1 API (key-free).
-- Error shape on failure: { ok=false, error="..." } — never leaks upstream body verbatim.

local API = "https://api.mangaupdates.com/v1"
local SITE = "https://www.mangaupdates.com"

local PREFERRED = { "English", "Romaji" }

-- Title Case like MangaDex tags ("Slice of Life", "Sci-Fi").
-- Preserve all-caps words (Sci-Fi, ISEKAI handled via hyphen rules) and known acronyms.

-- alt title candidates from the associated-title list; MangaUpdates exposes no language tag,
-- so lang is empty. Host de-dupes against MangaDex by normalized string compare.

local function post_json(url, body_tbl)
  local res, err = host.http.fetch(url, {
    method = "POST",
    body = json.encode(body_tbl),
    headers = {
      ["Content-Type"] = "application/json",
      ["Accept"] = "application/json",
    },
  })
  if err ~= nil then
    return nil
  end
  if not res or res.status ~= 200 then
    return nil
  end
  local ok, body = pcall(json.decode, res.body)
  if not ok or type(body) ~= "table" then
    return nil
  end
  return body
end

local function get_json(url)
  return (host.http.get_json(url, { headers = { ["Accept"] = "application/json" } }))
end

local function normalize(s)
  return tostring(s or ""):lower():gsub("[^%w%s]", " "):gsub("%s+", " "):gsub("^%s+", ""):gsub("%s+$", "")
end

local function best_series(query)
  local q = normalize(query)
  if q == "" then
    return nil
  end
  local search = post_json(API .. "/series/search", { search = query })
  local results = search and search.results
  if type(results) ~= "table" or #results == 0 then
    return nil
  end

  -- Skip novels: the manga adaptation's record is the one to use.
  local record
  for _, hit in ipairs(results) do
    local rec = hit.record
    if rec and rec.type ~= "Novel" then
      record = rec
      break
    end
  end
  if not record then
    for _, hit in ipairs(results) do
      if hit.record then
        record = hit.record
        break
      end
    end
  end
  if not record then
    return nil
  end

  local full = get_json(API .. "/series/" .. tostring(("%d"):format(tonumber(record.series_id) or 0)))
  local data = type(full) == "table" and full or {}

  -- Guard against wrong-series matches: title must equal the record title or
  -- one of the associated titles (normalized). Loose-prefix fallback for
  -- romanization variants with matching 70%+ of 4+ char prefix-tolerant tokens.
  local wanted = q
  local function variant_match(candidate)
    local want_t, cand_t = {}, {}
    for w in wanted:gmatch("%S+") do
      want_t[#want_t + 1] = w
    end
    for w in normalize(candidate):gmatch("%S+") do
      cand_t[#cand_t + 1] = w
    end
    if #want_t < 3 then
      return normalize(candidate) == wanted
    end
    local used, matched = {}, 0
    for _, want in ipairs(want_t) do
      for i, have in ipairs(cand_t) do
        if
          not used[i]
          and (
            want == have
            or (#want >= 4 and #have >= 4 and (want:sub(1, #have) == have or have:sub(1, #want) == want))
          )
        then
          used[i] = true
          matched = matched + 1
          break
        end
      end
    end
    return matched >= #want_t * 0.7
  end

  local matched = variant_match(record.title)
  if not matched then
    for _, assoc in ipairs(data.associated or {}) do
      if variant_match(assoc.title) then
        matched = true
        break
      end
    end
  end
  if not matched then
    return nil
  end

  data.url = data.url or record.url or SITE
  data.genres = data.genres or record.genres
  if not data.description or data.description == "" then
    data.description = record.description
  end
  data.recommendations = data.recommendations or record.recommendations or {}
  data.related_series = data.related_series or record.related_series or {}
  return data
end

local function clean_text(s)
  s = tostring(s or "")
  s = s:gsub("%[([^%]]+)%]%(([^%)]+)%)", "%1")
  s = s:gsub("https?://%S+", "")
  s = s:gsub("\r\n", "\n")
  s = s:gsub("[ \t]+", " ")
  return (s:gsub("^%s+", "")):gsub("%s+$", "")
end

local function author_names(data)
  local out = {}
  local seen = {}
  for _, a in ipairs(data.authors or {}) do
    local name = a.name
    if name and name ~= "" and not seen[name] then
      seen[name] = true
      out[#out + 1] = name
    end
  end
  return out
end

local function title_case(s)
  local LOWER = {
    ["of"] = true,
    ["the"] = true,
    ["and"] = true,
    ["a"] = true,
    ["an"] = true,
    ["in"] = true,
    ["on"] = true,
    ["for"] = true,
    ["to"] = true,
  }
  local KEEP = { ["sci-fi"] = "Sci-Fi" }
  local lower = s:lower()
  if KEEP[lower] then
    return KEEP[lower]
  end
  local function cap_word(w)
    if w == "" then
      return w
    end
    return (w:gsub("([a-zA-Z])([a-zA-Z]*)", function(a, b)
      return a:upper() .. b:lower()
    end))
  end
  local out = {}
  for part in lower:gmatch("%S+") do
    if #out > 0 and LOWER[part] then
      out[#out + 1] = part
    else
      out[#out + 1] = cap_word(part)
    end
  end
  return table.concat(out, " ")
end

local function alt_titles(data)
  local out = {}
  local seen = {}
  local title = tostring(data.title or "")
  if title ~= "" and not seen[title] then
    seen[title] = true
    out[#out + 1] = { title = title, lang = "", main = true }
  end
  for _, a in ipairs(data.associated or {}) do
    local t = tostring(a.title or "")
    if t ~= "" and not seen[t] then
      seen[t] = true
      out[#out + 1] = { title = t, lang = "", main = false }
    end
  end
  return out
end

local function genre_names(data)
  local out = {}
  local seen = {}
  for _, g in ipairs(data.genres or {}) do
    local name = g.genre
    if name and name ~= "" then
      name = title_case(name)
      local key = name:lower()
      if not seen[key] then
        seen[key] = true
        out[#out + 1] = name
      end
    end
  end
  return out
end

local function cover_url(data)
  local image = data.image or {}
  local url = image.url and image.url.original
  if type(url) == "string" and url ~= "" then
    return url
  end
  return nil
end

return {
  API = API,
  SITE = SITE,
  PREFERRED = PREFERRED,
  post_json = post_json,
  get_json = get_json,
  normalize = normalize,
  best_series = best_series,
  clean_text = clean_text,
  author_names = author_names,
  title_case = title_case,
  alt_titles = alt_titles,
  genre_names = genre_names,
  cover_url = cover_url,
}
