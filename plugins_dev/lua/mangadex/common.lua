-- MangaDex plugin: search/detail/chapters via api.mangadex.org, using only
-- host.http.fetch + json + base/string/table libs. Canonical output fields:
-- {id, name, cover, status, tags, release_at, chapters = {id, number, title, release_at}}
-- canonical status; unknown passes through, empty -> "unknown"
local normalize_status = host.text.normalize_status

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

-- RFC3339 timestamps -> unix seconds via host.text.date_epoch (D7); 0 when
-- absent/unparseable, the house zero-time contract.
local function epoch(s)
  return host.text.date_epoch(tostring(s or "")) or 0
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
  local authors, artists = {}, {}
  local desc = attrs.description and attrs.description.en or ""
  for _, rel in ipairs(m.relationships or {}) do
    if (rel.type == "author" or rel.type == "artist") and rel.attributes then
      local nm = rel.attributes.name
      if nm then
        if rel.type == "author" then authors[#authors + 1] = nm
        else artists[#artists + 1] = nm end
      end
    end
  end
  return {
    id = m.id,
    name = name,
    cover = cover,
    status = normalize_status(attrs.status),
    tags = tags,
    release_at = epoch(attrs.createdAt),
    description = desc,
    authors = authors,
    artists = artists,
    url = "https://mangadex.org/title/" .. (m.id or ""),
  }
end


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
return { normalize_status=normalize_status, API=API, AUTH=AUTH, have_creds=have_creds, epoch=epoch, token_post=token_post, access_token=access_token, get=get, canonical_manga=canonical_manga, tag_ids=tag_ids }
