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
