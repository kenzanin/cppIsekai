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
