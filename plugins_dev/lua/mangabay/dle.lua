-- dle.lua — DataLifeEngine guard + payload helpers for manga-bay.
-- Port of goIsekai examples/plugins/lua/mangabay/dle.lua; the PoW guard is
-- solved over plain HTTP (host.http.fetch, per-plugin cookie jar via
-- cookie=true). json_blob inlines the host JSONBlob extractor (cppIsekai has
-- no host.text.json_blob): first balanced {} / [] after the marker, quotes
-- and escapes tracked.
local dle = {}

dle.UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36"
dle.ACCEPT_LANG = "en-US,en;q=0.9"

function dle.headers(extra)
  local h = { ["User-Agent"] = dle.UA, ["Accept-Language"] = dle.ACCEPT_LANG }
  if extra then for k, v in pairs(extra) do h[k] = v end end
  return h
end

-- nonce whose sha256(token:nonce) has hex prefix "00" (the guard's difficulty);
-- the token is the raw page string, not the url-decoded form.
function dle.pow_nonce(token)
  if not token then return nil end
  for i = 0, 60000 do
    local h = host.crypto.sha256_hex(token .. ":" .. i)
    if h:sub(1, 2) == "00" then return i, h end
  end
  return nil
end

-- GET target -> 404 challenge page -> pow -> POST /_v; the trust cookie lands
-- in the per-plugin jar (cookie=true on every call). No-op on normal pages
-- (no "pow_nonce" token there).
function dle.solve_guard(site, body)
  if not body or not body:find("pow_nonce", 1, true) then return false end
  local m = host.re2.find([[token:\s*"([^"]+)"]], body)
  if #m == 0 or m[2] == "" then return false end
  local nonce, hash = dle.pow_nonce(m[2])
  if not nonce then return false end
  local form = "token=" .. host.url_encode(m[2])
    .. "&mode=modern&workTime=55&iterations=110&hasCrypto=1&gms=-1&fi=1&rtc=98"
    .. "&rtip=" .. dle.client_ip()
    .. "&pow_nonce=" .. nonce .. "&pow_hash=" .. hash
    .. "&webdriver=0&touch=1&screen_w=1856&screen_h=1044&screen_cd=24"
    .. "&wgv=AMD&wgr=Radeon&tz=-420&dpr=1&cdp=0&cdpf="
  host.http.fetch(site .. "/_v", {
    method = "POST",
    body = form,
    headers = dle.headers({ Origin = site }),
    cookie = true,
  })
  return true
end

-- the server accepts any non-empty rtip; an empty one escalates to Turnstile.
function dle.client_ip()
  local ip = host.plugin.param("rtip")
  if ip and ip ~= "" then return ip end
  return "1.2.3.4"
end

-- GET site..path, solving the guard when the response is the challenge page
-- (a 404, so the status is not checked here — the body shape decides).
function dle.get(site, path)
  local res = host.http.fetch(site .. path, { headers = dle.headers(), cookie = true })
  local body = res and res.body or ""
  if dle.solve_guard(site, body) then
    res = host.http.fetch(site .. path, { headers = dle.headers(), cookie = true })
    body = res and res.body or ""
  end
  return body
end

-- first balanced {...} / [...] after marker ("" searches from the start),
-- decoded; nil when absent or malformed. Port of pluginutil.JSONBlob.
function dle.json_blob(text, marker)
  text = tostring(text or "")
  if marker and marker ~= "" then
    local at = text:find(marker, 1, true)
    if not at then return nil end
    text = text:sub(at + #marker)
  end
  local at, _, open = text:find("([%{%[])")
  if not at then return nil end
  local closer = open == "[" and "]" or "}"
  local depth, in_str, esc = 0, false, false
  for i = at, #text do
    local c = text:sub(i, i)
    if in_str then
      if esc then
        esc = false
      elseif c == "\\" then
        esc = true
      elseif c == '"' then
        in_str = false
      end
    elseif c == '"' then
      in_str = true
    elseif c == open then
      depth = depth + 1
    elseif c == closer then
      depth = depth - 1
      if depth == 0 then
        local ok, data = pcall(json.decode, text:sub(at, i))
        if ok and type(data) == "table" then return data end
        return nil
      end
    end
  end
  return nil
end

-- "18.10.2023" (d.m.yyyy) -> unix midnight UTC (Hinnant civil-days; the
-- sandbox has no os.date). 0 when the date is absent/malformed.
function dle.dle_date(s)
  local d, m, y = string.match(tostring(s or ""), "(%d+)%.(%d+)%.(%d+)")
  if not y then return 0 end
  d, m, y = tonumber(d), tonumber(m), tonumber(y)
  local yy = y - (m <= 2 and 1 or 0)
  local era = math.floor(yy / 400)
  local yoe = yy - era * 400
  local doy = math.floor((153 * (m + (m > 2 and -3 or 9)) + 2) / 5) + d - 1
  local doe = yoe * 365 + math.floor(yoe / 4) - math.floor(yoe / 100) + doy
  return (era * 146097 + doe - 719468) * 86400
end

-- numeric id + slug from a DLE permalink (.../29706-thor-1966.html).
function dle.slug_id(href)
  if not href then return nil, nil end
  local id, slug = string.match(tostring(href), "/(%d+)-([^/]-)%.html")
  if id then return tonumber(id), slug end
  return nil, nil
end

-- ComicSeries node of a JSON-LD @graph block, for description/genre fallback.
function dle.comic_series(html)
  for _, m in ipairs(host.re2.find_all([[(?s)<script type="application/ld\+json">(.*?)</script>]], html)) do
    local ok, doc = pcall(json.decode, m[2] or "")
    if ok and type(doc) == "table" and type(doc["@graph"]) == "table" then
      for _, node in ipairs(doc["@graph"]) do
        if type(node) == "table" and node["@type"] == "ComicSeries" then return node end
      end
    end
  end
  return nil
end

return dle
