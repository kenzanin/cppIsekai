local c = require("common")
local API = c.API
local SITE = c.SITE
local PREFERRED = c.PREFERRED
local post_json = c.post_json
local get_json = c.get_json
local normalize = c.normalize
local best_series = c.best_series
local clean_text = c.clean_text
local author_names = c.author_names
local title_case = c.title_case
local alt_titles = c.alt_titles
local genre_names = c.genre_names
local cover_url = c.cover_url

function handle(req)
  local p = req.params or {}
  local title = tostring(p.title or "")
  local kind = tostring(p.kind or "detail")
  if title == "" then return { ok = false, error = "missing title" } end
  if kind ~= "detail" and kind ~= "covers" and kind ~= "recommendations" then return { ok = false, error = "kind must be detail|covers|recommendations" } end
  if #title > 512 then return { ok = false, error = "title too long" } end

  local data = best_series(title)
  if not data then return { ok = false, error = "no match found" } end

  if kind == "covers" then
    local url = cover_url(data)
    return {
      ok = true,
      data = {
        source_url = data.url,
        covers = url and { url } or {},
      },
    }
  end

  if kind == "recommendations" then
    -- Two MangaUpdates sections carry related reading: `recommendations`
    -- (weighted) and `related_series` (spin-off / side-story relations).
    -- Either can be empty alone, so merge both and de-duplicate by series id.
    local function id_str(v)
      local n = tonumber(v)
      if n ~= nil and n == math.floor(n) then return string.format("%d", n) end
      return tostring(v or "")
    end
    local recs, seen = {}, {}
    local function add(name, url, sid, weight)
      name = tostring(name or "")
      if name == "" then return end
      url = tostring(url or SITE)
      sid = id_str(sid)
      local key = sid ~= "" and sid or url
      if seen[key] then return end
      seen[key] = true
      recs[#recs + 1] = { name = name, url = url, id = sid, weight = weight }
    end
    for _, r in ipairs(data.recommendations or {}) do
      add(r.series_name, r.series_url, r.series_id, tonumber(r.weight) or 0)
    end
    for _, r in ipairs(data.related_series or {}) do
      add(r.related_series_name, r.related_series_url, r.related_series_id, -1)
    end
    table.sort(recs, function(a, b) return a.weight > b.weight end)
    for _, r in ipairs(recs) do r.weight = nil end
    return { ok = true, data = { items = recs } }
  end

  return {
    ok = true,
    data = {
      title = tostring(data.title or ""),
      synopsis = clean_text(data.description),
      authors = author_names(data),
      tags = genre_names(data),
      alt_titles = alt_titles(data),
      source_url = data.url,
    },
  }
end
