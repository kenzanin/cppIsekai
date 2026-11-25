-- comix.to: shared helpers (site-specific, mirrored from main.lua split).
local BASE = host.plugin.param("site_url") or "https://comix.to"
local RENDER = { profile = "lightpanda", timeout_ms = 60000 }

-- ── helpers ────────────────────────────────────────────────────────────────

local function render(url)
  local res, err = host.http.fetch(url, RENDER)
  if err ~= nil then
    return nil, err
  end
  if res.status ~= 200 then
    return nil, "comix: upstream status " .. tostring(res.status)
  end
  return res.body, nil
end

-- first capture group of the first match; nil when no match (host re2.capture).
local function find1(s, pattern)
  return host.re2.capture(pattern, tostring(s or ""))
end

-- cover_abs absolutizes bare /images/covers/... paths (both the SSR detail
-- JSON and the browse cards emit them); a relative cover would resolve
-- against the host instead of comix.
local function cover_abs(url)
  if type(url) ~= "string" or url == "" then
    return ""
  end
  if url:sub(1, 1) == "/" then
    return BASE .. url
  end
  return url
end

-- The detail/genre payload is embedded in script#initial-data (JSON).
local function initial_data(html)
  return find1(html or "", [[(?s)<script type="application/json" id="initial-data">(.*?)</script>]])
end

-- comix renders relative chapter dates ("2mos ago"); host.text.date_to_iso
-- parses the long forms, so short aliases normalize first (unit map mirrors
-- the reference comix extension). Returns "" when unparseable -> host zero.
local ALIAS = {
  m = "minute",
  min = "minute",
  mins = "minute",
  h = "hour",
  hr = "hour",
  hrs = "hour",
  d = "day",
  day = "day",
  days = "day",
  w = "week",
  week = "week",
  weeks = "week",
  mo = "month",
  mos = "month",
  month = "month",
  months = "month",
  y = "year",
  yr = "year",
  yrs = "year",
  year = "year",
  years = "year",
}

local function relative_to_iso(s)
  s = host.text.trim(s or "")
  if s == "" then
    return ""
  end
  local iso = host.text.date_to_iso(s)
  if iso then
    return iso
  end
  local n, unit = string.match(string.lower(s), "^%s*(%d+)%s*([%a]+)%s+ago%s*$")
  if not n then
    return ""
  end
  local long = ALIAS[unit]
  if not long then
    return ""
  end
  return host.text.date_to_iso(n .. " " .. long .. " ago") or ""
end
return {
  BASE = BASE,
  render = render,
  find1 = find1,
  cover_abs = cover_abs,
  initial_data = initial_data,
  relative_to_iso = relative_to_iso,
}
