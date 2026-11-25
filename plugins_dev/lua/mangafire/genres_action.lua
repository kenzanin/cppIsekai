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
return function()
  local names = require("genres").names
  return { ok = true, data = names }
end
