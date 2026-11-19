-- comik.to: translation-hub aggregator (manga grouped by scanlation source),
-- pure Qwik SSR. Port of goIsekai examples/plugins/lua/comik to the cppIsekai
-- host ABI (host.http.fetch + status check; host.re2.* pattern-first).
-- Pages are plain URL strings; chapter release_at is unix seconds (house
-- mangadex precedent) instead of goIsekai's RFC3339 string requirement.
local BASE = "https://comik.to"

-- host.text.trim: utf8-aware, case-preserving (plugin-host-promotion).
local trim = host.text.trim

-- http request -> (body, err): the host two-value fetch_body form.
local http_req = host.http.fetch_body

-- re2 find_all (pattern, body); nil on bad pattern (pcall) so callers can skip.
local function find_all(body, pattern)
  local ok, res = pcall(host.re2.find_all, pattern, tostring(body or ""))
  if not ok then return nil end
  return res
end

-- <time data-time="..."> epoch ms -> unix seconds; nil when absent.
local function epoch_ms_to_secs(ms)
  local n = tonumber(ms)
  if not n then return nil end
  return math.floor(n / 1000)
end

local methods = {}

-- ── search ─────────────────────────────────────────────────────────────────
-- Cards: <a href="/title/<id>" : class="block relative"><img alt="<title>"
-- src="<cover>"...>; the anchor repeats with the alias list, so dedup by id.
methods.search = function(params)
  local query = tostring(params.title or params.query or params.q or "")
  if query == "" then
    return { ok = false, error = "comik: empty query" }
  end
  local body, err = http_req(BASE .. "/search?word=" .. host.url_encode(query))
  if not body then
    return { ok = false, error = err or "comik: search failed" }
  end

  local items, seen = {}, {}
  for _, m in ipairs(find_all(body,
    [[(?s)<a href="/title/([a-z0-9]+)" : class="block relative"><img[^>]*alt="([^"]*)"[^>]*src="([^"]+)"]]
  ) or {}) do
    local id, title, cover = m[2], m[3], m[4]
    if id and not seen[id] then
      seen[id] = true
      items[#items + 1] = {
        id = id,
        name = host.text.unescape(trim(title)),
        cover = cover:find("^http") and cover or (BASE .. cover),
      }
    end
  end
  return { ok = true, data = { items = items, total = #items } }
end

-- ── detail ─────────────────────────────────────────────────────────────────
-- <title> carries the display name (attrs allowed on this site); genre chips
-- are plain text; status is a rendered Completed/Ongoing badge.
methods.detail = function(params)
  local id = tostring(params.id or "")
  if id == "" then
    return { ok = false, error = "comik: missing id" }
  end
  local body, err = http_req(BASE .. "/title/" .. id)
  if not body then
    return { ok = false, error = err or "comik: detail failed" }
  end

  local name = host.text.unescape(trim(
    (body:match("<title[^>]*>([^<]*)</title>") or id):gsub("%s*::%s*COMIK%.TO%s*$", "")))

  -- first large _f webp in the page header area
  local cover = body:match('<img[^>]*src="(/_f/[^"]+%.webp)"') or ""

  -- genre chips are bare words; keep the known-genre set (source parity)
  local genres, gseen = {}, {}
 for g in body:gmatch(">(%u%a+)<") do
    local gl = g:lower()
    if not gseen[gl] and (gl == "action" or gl == "adventure" or gl == "comedy"
      or gl == "drama" or gl == "fantasy" or gl == "romance" or gl == "isekai"
      or gl == "horror" or gl == "mystery" or gl == "sci" or gl == "slice"
      or gl == "sports" or gl == "supernatural" or gl == "thriller") then
      gseen[gl] = true
      genres[#genres + 1] = gl
    end
  end

  local status = body:match(">(Ongoing)<") and "ongoing"
    or (body:match(">(Completed)<") and "completed" or "")

  return { ok = true, data = {
    id = id,
    name = name,
    cover = cover:find("^http") and cover or (cover ~= "" and BASE .. cover or ""),
    status = host.text.normalize_status(status),
    tags = genres,
    -- no machine date on the title page
    release_at = 0,
    description = "",
    authors = {},
    artists = {},
    url = BASE .. "/title/" .. id,
  } }
end

-- ── chapters ───────────────────────────────────────────────────────────────
-- Translation hub: the title page lists /source/<id> groups plus a few latest
-- chapters; full lists live on each source page. Merge all, dedup by chapter
-- ID (labels "Oneshot" and "Chapter 0" both coerce to 0 but are distinct),
-- keep the earliest release per chapter.
methods.chapters = function(params)
  local id = tostring(params.id or "")
  if id == "" then
    return { ok = false, error = "comik: missing id" }
  end
  local body, err = http_req(BASE .. "/title/" .. id)
  if not body then
    return { ok = false, error = err or "comik: chapters failed" }
  end

  local srcs, sseen = {}, {}
  for sid in body:gmatch('href="/source/([a-z0-9]+)"') do
    if not sseen[sid] then
      sseen[sid] = true
      srcs[#srcs + 1] = sid
    end
  end

  -- Chapter row: <a href="/chapter/<id>" ...><label></a> ... <time data-time>.
  -- Plain Lua string ops: coregex lazy quantifiers proved unreliable here
  -- (source lesson, kept). Anchor on the link, read data-time within +400.
  local out, bynum = {}, {}
  local function harvest(pagebody)
    local pos = pagebody:find('href="/chapter/', 1, true)
    while pos do
      local cid = pagebody:match('/chapter/([a-z0-9]+)', pos + 15)
      local close = pagebody:find('</a>', pos, true)
      if not cid or not close then break end
      local window = pagebody:sub(pos, close + 400)
      local ms = window:match('data%-time="(%d+)"') or window:match('data-time="(%d+)"')
      local label = window:match('>([^<]+)</a>')
      if ms then
        local secs = epoch_ms_to_secs(ms)
        local prev = bynum[cid]
        if prev then
          -- keep the earliest release across groups
          if secs and (not prev.release_at or secs < prev.release_at) then
            prev.release_at = secs
          end
        else
          local num = label and label:match("(%d+%.?%d*)") or "0"
          local item = {
            id = cid,
            number = tonumber(num) or 0,
            title = host.text.unescape(trim(label or "")),
            release_at = secs or 0,
            pages = 0,
          }
          bynum[cid] = item
          out[#out + 1] = item
        end
      end
      pos = pagebody:find('href="/chapter/', close, true)
    end
  end

  harvest(body) -- latest on the title page first (single-source titles)
  for _, sid in ipairs(srcs) do
    local sb = http_req(BASE .. "/source/" .. sid)
    if sb then harvest(sb) end
  end

  if #out == 0 then
    return { ok = false, error = "comik: no chapters for " .. id }
  end
  -- newest first (sources render newest-first; the title page mixes)
  table.sort(out, function(a, b) return (a.number or 0) > (b.number or 0) end)
  return { ok = true, data = out }
end

-- ── pages ──────────────────────────────────────────────────────────────────
-- Reader pages are SSR <img src="https://iXX.img00.org/...webp"> in order.
methods.pages = function(params)
  local chid = tostring(params.id or "")
  if chid == "" then
    return { ok = false, error = "comik: missing id" }
  end
  local body, err = http_req(BASE .. "/chapter/" .. chid)
  if not body then
    return { ok = false, error = err or "comik: reader fetch failed" }
  end

  local out = {}
  for u in body:gmatch('https://i%d+%.img00%.org/[^"]+%.webp') do
    out[#out + 1] = u
  end
  if #out == 0 then
    return { ok = false, error = "comik: no pages for " .. chid }
  end
  return { ok = true, data = out }
end

-- ── genres ─────────────────────────────────────────────────────────────────
methods.genres = function()
  return { ok = true, data = require("genres").names }
end

function handle(req)
  local fn = methods[req.method]
  if fn == nil then return { ok = false, error = "unknown method " .. tostring(req.method) } end
  return fn(req.params or {})
end
