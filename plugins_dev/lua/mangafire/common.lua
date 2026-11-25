-- MangaFire plugin (mangafire.to): VRF-signed JSON API, CDN covers.
-- Port of goIsekai examples/plugins/lua/mangafire; see knowledge/references/goisekai-abi-mapping
-- and references/mangafire-vrf-algorithm. VRF tables stay plugin-side (site secret).
-- Envelope { ok, data | error }; canonical manga fields per the mangadex plugin.
local API_URL = "https://mangafire.to/api"
local REFERER = "https://mangafire.to/"
local USER_AGENT =
  "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36"

local VRF_K1 = "0Ec58JOY3uBzJK9m3zqIOpdlF7UFiax9DmA="
local VRF_T1 =
  "yINlmUNho8VYJT+ibTIP+9ESiULpVEtMOoD6U6lRE0R/xwXo/Xp9NrUgC4cw/Lmo33vUyjUE40kUoEWIr/fxfNNcq2s79ShQ5NhNrFnJ4hXPwOu/SuXzIbuTQKGFvfm08E9jvCfqAtoDqvQq3dVWPQFmJjgvkISBeXY3BgANR+yVnjGbcxZ47d6kLNfZPIayTq3/YGySb1KuVZodWp/WGNAO5pfMcpaK53Hhs0allBszaMaxuouOwdxbwgxIw6YunSsXjI05Yi0j9j4eHKfSXR8Ifo/Od+8iamRfCXTyvm7NGRGYdcQ0ywcK/u6RXhrbcCm4t2eCtrDgQVecJGkQ+A=="
local VRF_K2 = "AAdjb1iPY8CiDmq9H34tKTBF8a3oDQ=="
local VRF_T2 =
  "IUFltCxD3Oc2cwCgkJffthaOg9cgPUb0LgW6H/VtfcF0kc5F25t+aWj6JH9VOhOaY0rAFdUxlDnl5BLNvwEJvQtP5qcw7vdb/K+chnbwnspSHT8mz5lqwz41TezG0hkO06FTjJZhsyNuFLDpD2ZZxQj/QIRcF90zpmQ7Byu483WsQqUE0C342HL+JXngRB6fRzxRyVTaKu83h7UYTJ0QMt6ixFh6S3F8gqkKwrGTL3jHNBsD45UnifK8+RGtishQV2K3rujLKEkiZxpr2dYcudFW4oFsDKhad3CLBvuyTqsCo4B7mL5IKQ1vXo/MOOvq1I1d8ar9X6Ttu5KF4fZgiA=="
local VRF_K3 = "DELOJgPsVaCcblDtTGMdHzM="
local VRF_T3 =
  "NQHlu1/wVO5EmkwQymF810qqY2xG1k2obcas4Z9mCsPEIFl9pRIjFxbJ7ybMHbBckT5Ton85E0FOeHezbh/mjlEYpmpnlXOS8dgrqeq2KfxImTh1YK9y0PeMNhzA1OQzSY9brYOJq/l2QnE/hwOeZIhPixVSKIUlDb5vLcH6RWKxkIEMuP0bDwIqQ71AJJaEaMJL7A6YtyIwoRT+L5v4aZzodN/0+3nOGsfblFjgxSfPzVDjNFeNl5P26+kEC/8AHgdrpAbt3hHz3HrRN1Y6e+JHgF7ncFWnoF0y3THL1S71WgWGCa6KtSzTCCG58n68nTyj2T3Sshk7utqCtMi/ZQ=="

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
  if err ~= nil then
    return nil, err
  end
  local keys = {}
  for k in pairs(send) do
    keys[#keys + 1] = k
  end
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
  if err ~= nil then
    return nil, err
  end
  if res.status ~= 200 then
    return nil, "upstream status " .. tostring(res.status) .. ": " .. tostring(res.body):sub(1, 80)
  end
  local ok, data = pcall(json.decode, res.body)
  if not ok or type(data) ~= "table" then
    return nil, "mangafire: invalid JSON"
  end
  return data, nil
end

local function sanitize_title(s)
  return (host.text.strip_html(tostring(s or "")):gsub("%s+", " "))
end

local function strip_html(s)
  return sanitize_title(s)
end

local normalize_status = host.text.normalize_status

local GENRE_IDS = require("genres").map

-- host pages envelope is a plain list of URL strings; per-page Referer headers
-- (img-r1.2xstorage.com requires Referer: https://mangafire.to/) cannot be
-- expressed — known gap, see references/goisekai-abi-mapping.
local function cover_of(poster)
  if type(poster) == "table" then
    return poster.medium or poster.large or poster.small or ""
  end
  return tostring(poster or "")
end

return {
  API_URL = API_URL,
  REFERER = REFERER,
  USER_AGENT = USER_AGENT,
  VRF_K1 = VRF_K1,
  VRF_T1 = VRF_T1,
  VRF_K2 = VRF_K2,
  VRF_T2 = VRF_T2,
  VRF_K3 = VRF_K3,
  VRF_T3 = VRF_T3,
  VRF_STAGES = VRF_STAGES,
  vrf_url = vrf_url,
  http_get = http_get,
  sanitize_title = sanitize_title,
  strip_html = strip_html,
  normalize_status = normalize_status,
  GENRE_IDS = GENRE_IDS,
  cover_of = cover_of,
}
