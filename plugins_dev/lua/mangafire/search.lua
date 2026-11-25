local c = require("common")
local API_URL = c.API_URL
local REFERER = c.REFERER
local USER_AGENT = c.USER_AGENT
local VRF_K1 = c.VRF_K1
local VRF_T1 = c.VRF_T1
local VRF_K2 = c.VRF_K2
local VRF_T2 = c.VRF_T2
local VRF_K3 = c.VRF_K3
local VRF_T3 = c.VRF_T3
local VRF_STAGES = c.VRF_STAGES
local vrf_url = c.vrf_url
local http_get = c.http_get
local sanitize_title = c.sanitize_title
local strip_html = c.strip_html
local normalize_status = c.normalize_status
local GENRE_IDS = c.GENRE_IDS
local cover_of = c.cover_of
return function(params)
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
