-- MangaDex plugin: search/detail/chapters via api.mangadex.org, using only
-- host.http.fetch + json + base/string/table libs. Canonical output fields:
-- {id, name, cover, status, tags, release_at, chapters = {id, number, title, release_at}}
local API = "https://api.mangadex.org"
local AUTH = "https://auth.mangadex.org/realms/mangadex/protocol/openid-connect/token"

-- Optional auth: credentials in config.json next to main.json. Absent file ->
-- every call below runs unauthenticated, exactly as before.
local cid, csecret, cuser, cpass
if config then
  cid = config.get("client_id")
  csecret = config.get("client_secret")
  cuser = config.get("username")
  cpass = config.get("password")
end

local function have_creds()
  return cid ~= nil and csecret ~= nil and cuser ~= nil and cpass ~= nil
end

-- POST the token endpoint (form-encoded). Returns token table or nil, err.
local function token_post(fields)
  local pairs_ = {}
  for k, v in pairs(fields) do pairs_[#pairs_ + 1] = k .. "=" .. tostring(v):gsub("[^%w%.%-~_]", function(c)
    return string.format("%%%02X", c:byte())
  end) end
  local res, err = host.http.fetch(AUTH, {
    method = "POST",
    body = table.concat(pairs_, "&"),
    headers = { ["Content-Type"] = "application/x-www-form-urlencoded" },
    accept = "application/json",
    user_agent = "cppIsekai/0.1",
  })
  if err ~= nil then return nil, err end
  if res.status ~= 200 then return nil, "auth status " .. res.status .. ": " .. res.body:sub(1, 80) end
  local ok, body = pcall(json.decode, res.body)
  if not ok or not body.access_token then return nil, "auth: malformed token response" end
  return body
end

-- Valid cached access_token, refreshing via stored refresh_token when past
-- expires_at. force=true skips the freshness check (401 path).
local function access_token(force)
  if not have_creds() then return nil end
  local tok = host.storage.get("access_token")
  local exp = tonumber(host.storage.get("expires_at")) or 0
  local refresh = host.storage.get("refresh_token")
  if tok ~= nil and not force and host.now() < exp - 30 then return tok end
  local body
  if refresh ~= nil then
    body = token_post({ grant_type = "refresh_token", refresh_token = refresh, client_id = cid, client_secret = csecret })
  end
  if body == nil then -- no refresh token yet, or refresh rejected: full login
    local err
    body, err = token_post({ grant_type = "password", username = cuser, password = cpass, client_id = cid,
      client_secret = csecret })
    if body == nil then return nil, err end
  end
  host.storage.set("access_token", body.access_token)
  host.storage.set("refresh_token", body.refresh_token or "")
  host.storage.set("expires_at", tostring(host.now() + (tonumber(body.expires_in) or 900)))
  return body.access_token
end

local function get(path, query)
  local url = API .. path
  if query and next(query) ~= nil then
    local parts = {}
    for k, v in pairs(query) do
      if type(v) == "table" then -- repeated key: includedTags[]=a&includedTags[]=b
        for _, vv in ipairs(v) do
          parts[#parts + 1] = k .. "=" .. tostring(vv):gsub(" ", "+"):gsub("%[", "%%5B"):gsub("%]", "%%5D")
        end
      else
        parts[#parts + 1] = k .. "=" .. tostring(v):gsub(" ", "+"):gsub("%[", "%%5B"):gsub("%]", "%%5D")
      end
    end
    url = url .. "?" .. table.concat(parts, "&")
  end
  -- Auth header ONLY on api.mangadex.org (never image/upload domains, per
  -- docs); authenticated responses are not cacheable, so public calls stay
  -- header-free.
  local opts = { accept = "application/json", user_agent = "cppIsekai/0.1" }
  if have_creds() then
    local at = access_token(false)
    if at ~= nil then opts.headers = { Authorization = "Bearer " .. at } end
  end
  -- MangaDex serves the bot-challenge page to browser UAs on API endpoints;
  -- an app UA gets clean JSON. accept=json keeps content negotiation honest.
  local res, err = host.http.fetch(url, opts)
  if err ~= nil then return nil, err end
  local ct = (res.headers and res.headers["content-type"]) or "?"
  host.log.info("GET " .. url .. " -> " .. res.status .. " len=" .. #res.body .. " ct=" .. ct)
  if res.status == 401 and have_creds() then
    -- stale-but-unexpired token: refresh once, retry once, then surface the error
    local at = access_token(true)
    if at ~= nil then
      opts.headers = { Authorization = "Bearer " .. at }
      res, err = host.http.fetch(url, opts)
      if err ~= nil then return nil, err end
      host.log.info("GET(retry) " .. url .. " -> " .. res.status)
    end
  end
  if res.status ~= 200 then return nil, "upstream status " .. res.status .. ": " .. res.body:sub(1, 80) end
  local body = json.decode(res.body)
  if body.result ~= "ok" then return nil, "upstream result " .. tostring(body.result) end
  return body
end

-- ISO-8601 (MangaDex timestamps) -> integer unix seconds (pure arithmetic; no os in sandbox).
local function epoch(s)
  if not s then return 0 end
  local y, mo, d, h, mi, sec = s:match("(%d+)-(%d+)-(%d+)T(%d+):(%d+):(%d+)")
  if not y then return 0 end
  y, mo, d, h, mi, sec = tonumber(y), tonumber(mo), tonumber(d), tonumber(h), tonumber(mi), tonumber(sec)
  -- days since civil epoch (Howard Hinnant's algorithm)
  local yy = y - (mo <= 2 and 1 or 0)
  local era = math.floor(yy / 400)
  local yoe = yy - era * 400
  local doy = math.floor((153 * (mo + (mo > 2 and -3 or 9)) + 2) / 5) + d - 1
  local doe = yoe * 365 + math.floor(yoe / 4) - math.floor(yoe / 100) + doy
  local days = era * 146097 + doe - 719468
  return days * 86400 + h * 3600 + mi * 60 + sec
end

local function canonical_manga(m)
  local attrs = m.attributes or {}
  local tags = {}
  for _, t in ipairs(attrs.tags or {}) do tags[#tags + 1] = t.attributes.name.en end
  local title = attrs.title or {}
  local name = title.en or title["ja-ro"] or title.ja or "untitled"
  if name == "untitled" then -- title-less works carry names in altTitles
    for _, alt in ipairs(attrs.altTitles or {}) do
      name = alt.en or alt["ja-ro"] or alt.ja
      if name then break end
    end
  end
  local cover = ""
  for _, rel in ipairs(m.relationships or {}) do
    if rel.type == "cover_art" then
      local fa = rel.attributes or {}
      cover = "https://uploads.mangadex.org/covers/" .. m.id .. "/" .. (fa.fileName or "") .. ".256.jpg"
    end
  end
  return {
    id = m.id,
    name = name,
    cover = cover,
    status = attrs.status or "",
    tags = tags,
    release_at = epoch(attrs.createdAt),
  }
end

local methods = {}

-- Tag name -> uuid map (genres source too); fetched once per plugin load.
local tag_map
local function tag_ids()
  if tag_map then return tag_map end
  local body, err = get("/manga/tag", { limit = 100 })
  if err ~= nil then return nil, err end
  local map = {}
  for _, t in ipairs(body.data or {}) do
    local name = t.attributes and t.attributes.name and t.attributes.name.en
    if name then map[name] = t.id end
  end
  tag_map = map
  return map
end

methods.search = function(params)
  local q = params.title or params.query or params.q or ""
  local page = math.max(1, tonumber(params.page) or 1)
  local limit = tonumber(params.limit) or 12
  -- followCount desc = the site's ranking (one piece -> One Piece first);
  -- bare title order is server-default and buried the obvious match.
  local query = { title = q, limit = limit, ["order[followedCount]"] = "desc", offset = (page - 1) * limit,
    ["includes[]"] = "cover_art" }
  if params.tags and #params.tags > 0 then -- "Isekai,Fantasy" -> includedTags[] uuid, AND
    local map, err = tag_ids()
    if err ~= nil then return { ok = false, error = err } end
    local uuids = {}
    for name in params.tags:gmatch("[^,]+") do
      local id = map[name:gsub("^%s+", ""):gsub("%s+$", "")]
      if id then uuids[#uuids + 1] = id end
    end
    if #uuids > 0 then query["includedTags[]"] = uuids end
  end
  local body, err = get("/manga", query)
  if err ~= nil then return { ok = false, error = err } end
  local items = {}
  for _, m in ipairs(body.data or {}) do items[#items + 1] = canonical_manga(m) end
  return { ok = true, data = { items = items, total = tonumber(body.total) or #items } }
end

methods.genres = function()
  local map, err = tag_ids()
  if err ~= nil then return { ok = false, error = err } end
  local names = {}
  for name in pairs(map) do names[#names + 1] = name end
  table.sort(names)
  return { ok = true, data = names }
end

methods.detail = function(params)
  local body, err = get("/manga/" .. (params.id or ""), { ["includes[]"] = "cover_art" })
  if err ~= nil then return { ok = false, error = err } end
  return { ok = true, data = canonical_manga(body.data) }
end

methods.chapters = function(params)
  -- MangaDex wants array-style params: translatedLanguage[]=en, order[chapter]=asc.
  local body, err = get("/manga/" .. (params.id or "") .. "/feed",
    { ["translatedLanguage[]"] = params.lang or "en", limit = params.limit or 100, ["order[chapter]"] = "asc" })
  if err ~= nil then return { ok = false, error = err } end
  local out = {}
  for _, c in ipairs(body.data or {}) do
    local attrs = c.attributes or {}
    out[#out + 1] = {
      id = c.id,
      number = tonumber(attrs.chapter) or 0,
      title = attrs.title or "",
      release_at = epoch(attrs.publishAt),
      pages = tonumber(attrs.pages) or 0,
    }
  end
  return { ok = true, data = out }
end

-- Page images for a chapter: /at-home/server/{id} -> {baseUrl, chapter{hash,data[]}}.
-- data[] is the quality pass; dataSaver[] deliberately unused (dataSaver off).
-- baseUrl already carries the scheme (live: "https://<host>.mangadex.network").
methods.pages = function(params)
  local body, err = get("/at-home/server/" .. (params.id or ""))
  if err ~= nil then return { ok = false, error = err } end
  local data = type(body.chapter) == "table" and body.chapter.data
  if type(body.baseUrl) ~= "string" or type(data) ~= "table" or #data == 0 then
    return { ok = false, error = "at-home: malformed response" }
  end
  local prefix = body.baseUrl .. "/data/" .. body.chapter.hash .. "/"
  local out = {}
  for _, f in ipairs(data) do out[#out + 1] = prefix .. f end
  return { ok = true, data = out }
end

function handle(req)
  local fn = methods[req.method]
  if fn == nil then return { ok = false, error = "unknown method " .. tostring(req.method) } end
  return fn(req.params or {})
end
