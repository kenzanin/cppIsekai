-- MangaFire plugin (mangafire.to): VRF-signed JSON API, CDN covers.
-- Port of goIsekai examples/plugins/lua/mangafire; see knowledge/references/goisekai-abi-mapping
-- and references/mangafire-vrf-algorithm. VRF tables stay plugin-side (site secret).
-- Envelope { ok, data | error }; canonical manga fields per the mangadex plugin.
local API_URL = "https://mangafire.to/api"
local REFERER = "https://mangafire.to/"
local USER_AGENT = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36"

local VRF_K1 = "0Ec58JOY3uBzJK9m3zqIOpdlF7UFiax9DmA="
local VRF_T1 = "yINlmUNho8VYJT+ibTIP+9ESiULpVEtMOoD6U6lRE0R/xwXo/Xp9NrUgC4cw/Lmo33vUyjUE40kUoEWIr/fxfNNcq2s79ShQ5NhNrFnJ4hXPwOu/SuXzIbuTQKGFvfm08E9jvCfqAtoDqvQq3dVWPQFmJjgvkISBeXY3BgANR+yVnjGbcxZ47d6kLNfZPIayTq3/YGySb1KuVZodWp/WGNAO5pfMcpaK53Hhs0allBszaMaxuouOwdxbwgxIw6YunSsXjI05Yi0j9j4eHKfSXR8Ifo/Od+8iamRfCXTyvm7NGRGYdcQ0ywcK/u6RXhrbcCm4t2eCtrDgQVecJGkQ+A=="
local VRF_K2 = "AAdjb1iPY8CiDmq9H34tKTBF8a3oDQ=="
local VRF_T2 = "IUFltCxD3Oc2cwCgkJffthaOg9cgPUb0LgW6H/VtfcF0kc5F25t+aWj6JH9VOhOaY0rAFdUxlDnl5BLNvwEJvQtP5qcw7vdb/K+chnbwnspSHT8mz5lqwz41TezG0hkO06FTjJZhsyNuFLDpD2ZZxQj/QIRcF90zpmQ7Byu483WsQqUE0C342HL+JXngRB6fRzxRyVTaKu83h7UYTJ0QMt6ixFh6S3F8gqkKwrGTL3jHNBsD45UnifK8+RGtishQV2K3rujLKEkiZxpr2dYcudFW4oFsDKhad3CLBvuyTqsCo4B7mL5IKQ1vXo/MOOvq1I1d8ar9X6Ttu5KF4fZgiA=="
local VRF_K3 = "DELOJgPsVaCcblDtTGMdHzM="
local VRF_T3 = "NQHlu1/wVO5EmkwQymF810qqY2xG1k2obcas4Z9mCsPEIFl9pRIjFxbJ7ybMHbBckT5Ton85E0FOeHezbh/mjlEYpmpnlXOS8dgrqeq2KfxImTh1YK9y0PeMNhzA1OQzSY9brYOJq/l2QnE/hwOeZIhPixVSKIUlDb5vLcH6RWKxkIEMuP0bDwIqQ71AJJaEaMJL7A6YtyIwoRT+L5v4aZzodN/0+3nOGsfblFjgxSfPzVDjNFeNl5P26+kEC/8AHgdrpAbt3hHz3HrRN1Y6e+JHgF7ncFWnoF0y3THL1S71WgWGCa6KtSzTCCG58n68nTyj2T3Sshk7utqCtMi/ZQ=="

local VRF_STAGES = {
  { iv = 0x5A, key = VRF_K1, tbl = VRF_T1 },
  { iv = 0x35, key = VRF_K2, tbl = VRF_T2 },
  { iv = 0xBA, key = VRF_K3, tbl = VRF_T3 },
}

-- Build a VRF-signed API URL. Array params ("name[]") are sent as repeated raw
-- "name[]" query parameters while the signature covers the indexed form, so
-- genres_in[]=13&genres_in[]=79 is signed as genres_in[0]=13&genres_in[1]=79.
local function vrf_url(path, params)
  local sign, send = {}, {}
  for k, v in pairs(params or {}) do
    if type(v) == "table" then
      local base = k:sub(1, -3) -- strip trailing "[]"
      for i, item in ipairs(v) do
        local indexed = base .. "[" .. (i - 1) .. "]"
        sign[indexed] = tostring(item)
        send[indexed] = k
      end
    else
      sign[k] = tostring(v)
      send[k] = host.url_encode(k)
    end
  end
  local sig, err = host.crypto.vrf_sign(path, sign, VRF_STAGES)
  if err ~= nil then return nil, err end
  local keys = {}
  for k in pairs(send) do keys[#keys + 1] = k end
  table.sort(keys)
  local parts = {}
  for _, k in ipairs(keys) do
    parts[#parts + 1] = send[k] .. "=" .. host.url_encode(sign[k])
  end
  local u = API_URL .. path
  if #parts > 0 then
    return u .. "?" .. table.concat(parts, "&") .. "&vrf=" .. sig, nil
  end
  return u .. "?vrf=" .. sig, nil
end

local function http_get(url)
  local res, err = host.http.fetch(url, {
    accept = "application/json",
    user_agent = USER_AGENT,
    headers = { Referer = REFERER },
  })
  if err ~= nil then return nil, err end
  if res.status ~= 200 then return nil, "upstream status " .. tostring(res.status) .. ": " .. tostring(res.body):sub(1, 80) end
  local ok, data = pcall(json.decode, res.body)
  if not ok or type(data) ~= "table" then return nil, "mangafire: invalid JSON" end
  return data, nil
end

local function sanitize_title(s)
  s = tostring(s or ""):gsub("<[^>]*>", " ")
  s = s:gsub("&amp;", "&"):gsub("&lt;", "<"):gsub("&gt;", ">"):gsub("&quot;", '"'):gsub("&#39;", "'"):gsub("&nbsp;", " ")
  return (s:gsub("%s+", " "):gsub("^%s+", ""):gsub("%s+$", ""))
end

local function strip_html(s)
  return sanitize_title(s)
end

-- canonical status; unknown passes through, empty -> "unknown"
local function normalize_status(s)
  s = tostring(s or ""):lower():gsub("%s+", " "):gsub("^%s+", ""):gsub("%s+$", "")
  if s == "" then return "unknown" end
  if s == "ongoing" or s == "releasing" or s == "publishing" then return "Ongoing" end
  if s == "completed" or s == "finished" then return "Completed" end
  if s == "hiatus" or s == "on hiatus" then return "Hiatus" end
  if s == "dropped" or s == "cancelled" then return "Dropped" end
  if s == "upcoming" or s == "not yet released" then return "Upcoming" end
  return s
end


local GENRE_IDS = require("genres").map

-- host pages envelope is a plain list of URL strings; per-page Referer headers
-- (img-r1.2xstorage.com requires Referer: https://mangafire.to/) cannot be
-- expressed — known gap, see references/goisekai-abi-mapping.
local function cover_of(poster)
  if type(poster) == "table" then return poster.medium or poster.large or poster.small or "" end
  return tostring(poster or "")
end

local methods = {}

methods.search = function(params)
  local q = params.title or params.query or params.q or ""
  local page = math.max(1, tonumber(params.page) or 1)
  local req = { limit = "50", page = tostring(page) }
  -- keyword has to be left out entirely when nothing was typed: an empty value
  -- changes the parameter set the VRF signature is checked against and the API 403s.
  if q ~= "" then req.keyword = q end
  local tags = params.tags or {}
  if type(tags) == "string" then
    local list = {}
    for name in tags:gmatch("[^,]+") do list[#list + 1] = (name:gsub("^%s+", ""):gsub("%s+$", "")) end
    tags = list
  end
  if type(tags) == "table" and #tags > 0 then
    local ids = {}
    for _, name in ipairs(tags) do
      local id = GENRE_IDS[name]
      if id then ids[#ids + 1] = id end
    end
    if #ids > 0 then
      req["genres_in[]"] = ids
      req.genres_mode = "and"
    end
  end
  local url, verr = vrf_url("/titles", req)
  if verr ~= nil then return { ok = false, error = verr } end
  local body, err = http_get(url)
  if err ~= nil then return { ok = false, error = err } end
  local items = {}
  for _, it in ipairs(body.items or {}) do
    if type(it) == "table" and it.hid then
      items[#items + 1] = { id = it.hid, name = sanitize_title(it.title), cover = cover_of(it.poster) }
    end
  end
  local meta = body.meta or {}
  local total = tonumber(meta.total) or tonumber(meta.count) or #items
  return { ok = true, data = { items = items, total = total } }
end

methods.detail = function(params)
  local hid = params.id or ""
  if hid == "" then return { ok = false, error = "mangafire: missing id" } end
  local url, verr = vrf_url("/titles/" .. hid, nil)
  if verr ~= nil then return { ok = false, error = verr } end
  local body, err = http_get(url)
  if err ~= nil then return { ok = false, error = err } end
  local d = type(body) == "table" and body.data
  if type(d) ~= "table" then return { ok = false, error = "mangafire: no title data for " .. hid } end
  local genre_names, authors = {}, {}
  for _, g in ipairs(d.genres or {}) do
    if type(g) == "table" and g.title then genre_names[#genre_names + 1] = g.title end
  end
  for _, a in ipairs(d.authors or {}) do
    if type(a) == "table" and a.title then authors[#authors + 1] = a.title end
  end
  return {
    ok = true,
    data = {
      id = d.hid or hid,
      name = sanitize_title(d.title),
      cover = cover_of(d.poster),
      status = normalize_status(d.status),
      tags = genre_names,
      release_at = 0,
      description = strip_html(d.synopsisHtml or d.synopsis or ""),
      authors = type(authors) == "table" and authors or {},
      artists = {},
      url = "https://mangafire.to/title/" .. tostring(d.hid or hid),
    },
  }
end

methods.chapters = function(params)
  local hid = params.id or ""
  if hid == "" then return { ok = true, data = {} } end
  local out, page = {}, 1
  -- up to 3 pages (200/page), newest-first upstream; sort ascending like mangadex
  -- (host routes pages/CBZ by chapter number and expects reading order)
  while page <= 3 do
    local url, verr = vrf_url("/titles/" .. hid .. "/chapters", {
      language = "en", limit = "200", order = "desc", page = tostring(page), sort = "number",
    })
    if verr ~= nil then return { ok = false, error = verr } end
    local body, err = http_get(url)
    if err ~= nil then return { ok = false, error = err } end
    local items = body.items or {}
    if #items == 0 then break end
    for _, c in ipairs(items) do
      if type(c) == "table" and c.id then
        local num = tonumber(c.number) or 0
        local cid = tonumber(c.id)
        out[#out + 1] = {
          id = cid and string.format("%.0f", cid) or tostring(c.id),
          number = num,
          title = c.name or "",
          release_at = tonumber(c.createdAt or c.created_at) or 0,
          pages = tonumber(c.pages) or tonumber(c.pageCount) or 0,
        }
      end
    end
    local meta = body.meta or {}
    local has_next = meta.hasNext or meta.has_next
    local last_page = tonumber(meta.lastPage or meta.last_page) or 1
    if not has_next or page >= last_page then break end
    page = page + 1
  end
  table.sort(out, function(a, b) return (a.number or 0) < (b.number or 0) end)
  return { ok = true, data = out }
end

methods.pages = function(params)
  local chapter_id = params.id or ""
  if chapter_id == "" then return { ok = false, error = "mangafire: missing id" } end
  local url, verr = vrf_url("/chapters/" .. chapter_id, nil)
  if verr ~= nil then return { ok = false, error = verr } end
  local body, err = http_get(url)
  if err ~= nil then return { ok = false, error = err } end
  local raw = type(body) == "table" and type(body.data) == "table" and body.data.pages
  if type(raw) ~= "table" or #raw == 0 then return { ok = false, error = "mangafire: no pages for chapter " .. chapter_id } end
  local out = {}
  for _, p in ipairs(raw) do
    local u = type(p) == "table" and p.url
    if type(u) == "string" and u ~= "" then out[#out + 1] = u end
  end
  if #out == 0 then return { ok = false, error = "mangafire: no usable page urls" } end
  return { ok = true, data = out }
end

methods.genres = function()
  local names = require("genres").names
  return { ok = true, data = names }
end

function handle(req)
  local fn = methods[req.method]
  if fn == nil then return { ok = false, error = "unknown method " .. tostring(req.method) } end
  return fn(req.params or {})
end
