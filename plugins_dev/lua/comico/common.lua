-- comico.moe: comick-backed aggregator; catalogue mirrors comick, the reader
-- proxies every source. Port of goIsekai examples/plugins/lua/comico to the
-- cppIsekai host ABI (host.http.fetch + status check, json globals).
-- Pages are plain URL strings; the per-page Referer header is dropped (the
-- host image-proxy falls back to site_url, same as comizy).
local SITE = "https://comico.moe" -- single-domain aggregator
local API = SITE .. "/api"
local UA = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36"
local HEADERS = { ["User-Agent"] = UA, ["Accept"] = "application/json" }

-- host.text.trim: utf8-aware, case-preserving (plugin-host-promotion).
local trim = host.text.trim

local function abs_url(u)
  if type(u) ~= "string" or u == "" then return "" end
  return host.url_abs(SITE, u)
end

local function abs_url(u)
  if type(u) ~= "string" or u == "" then return "" end
  if u:sub(1, 4) == "http" then return u end
  return SITE .. u
end

-- [{id,name,group}] -> {"name", ...}
local function tags_to_names(tags)
  local out = {}
  for _, t in ipairs(tags or {}) do
    if type(t) == "table" and t.name then out[#out + 1] = tostring(t.name) end
  end
  return out
end

-- goIsekai took JSON-string args; cppIsekai params are tables (or bare ids).
local function decode_id(v)
  if type(v) == "table" then v = v.id end
  return v
end

-- GET + 200 + JSON decode -> table, or nil on any failure (goIsekai http_get).
local function http_get(url)
  local data
  for _ = 1, 4 do -- CF 502s are intermittent; same retry policy as POST
    local res, err = host.http.fetch(url, { headers = HEADERS })
    if err == nil and res.status == 200 then
      local ok, decoded = pcall(json.decode, res.body or "")
      if ok and type(decoded) == "table" then data = decoded break end
    end
  end
  return data
end

local function http_post_json(url, body)
  -- comico.moe's CF origin 502s intermittently on POSTs; retry a few times
  local last_err
  for _ = 1, 4 do
    local res, err = host.http.fetch(url, {
      method = "POST",
      body = body,
      headers = {
        ["User-Agent"] = UA,
        ["Accept"] = "application/json",
        ["Content-Type"] = "application/json",
        ["Referer"] = SITE .. "/",
      },
    })
    if err == nil and res.status == 200 then return res.body end
    last_err = err or res.status
  end
  host.log.info("comico post failed: " .. tostring(last_err) .. " " .. url)
  return nil
end


-- ── search ─────────────────────────────────────────────────────────────────
-- paginates:false in main.json => every match; API 400s above limit=100, so
-- walk 100-row slices (source parity). Only the first genre maps to the
-- single comickTags filter the API takes.



local function chapter_pages(base, into)
  local offset = 0
  for _ = 1, 20 do
    local data = http_get(base .. "&offset=" .. tostring(offset))
    if not data or type(data.items) ~= "table" or #data.items == 0 then break end
    for _, ch in ipairs(data.items) do
      into[#into + 1] = {
        id = tostring(ch.id or ""),
        number = tonumber(ch.number) or 0,
        title = trim(ch.title or ""),
        release_at = ch.publishedAt or 0, -- API ISO string, pass-through
        pages = 0,
      }
    end
    offset = offset + #data.items
    if offset >= (tonumber(data.total) or 0) then break end
  end
end


return { SITE=SITE, API=API, UA=UA, HEADERS=HEADERS, trim=trim, abs_url=abs_url, abs_url=abs_url, tags_to_names=tags_to_names, decode_id=decode_id, http_get=http_get, http_post_json=http_post_json, chapter_pages=chapter_pages }
