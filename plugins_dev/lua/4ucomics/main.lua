-- 4ucomics: private JSON API v1 behind Cloudflare (the HTML shell is a JS
-- SPA). API calls ride the lightpanda renderer profile -- the challenge
-- solver (decisions/renderer-fetch-engines). Page/cover CDNs vary per title
-- and some (mangadot.net) sit behind CF too: every image URL is wrapped in
-- the site's own /api/v1/img proxy, fetched by the host with the manifest
-- `image_engine` impersonation profile (image-engine-profile).
-- Envelope { ok, data | error }; canonical manga fields per the mangafire plugin.
local API = "https://4ucomics.com/api/v1"

local function proxy_img(u)
  if type(u) ~= "string" or u == "" then return u end
  return API .. "/img?url=" .. host.url_encode(u)
end

-- Site vocab (anilist-style) -> canonical lowercase; unknown falls to "unknown".
local STATUS = {
  RELEASING = "ongoing", ONGOING = "ongoing",
  FINISHED = "completed", COMPLETED = "completed",
  HIATUS = "hiatus",
  CANCELLED = "dropped", DROPPED = "dropped",
  NOT_YET_RELEASED = "upcoming", UPCOMING = "upcoming",
}

local function status_of(list)
  for _, s in ipairs(list or {}) do
    local m = STATUS[string.upper(tostring(s))]
    if m then return m end
  end
  return "unknown"
end

local function get_json(path)
  local res, err = host.http.fetch(API .. path, { profile = "lightpanda", timeout_ms = 60000 })
  if err ~= nil then return nil, err end
  if res.status ~= 200 then
    return nil, "4ucomics status " .. tostring(res.status) .. ": " .. tostring(res.body):sub(1, 80)
  end
  local ok, data
  local body = res.body
  if body:sub(1, 1) == "<" then body = body:match("<pre>(.*)</pre>") or body end -- renderer wraps json in <pre>
  ok, data = pcall(json.decode, body)
  if not ok or type(data) ~= "table" then return nil, "4ucomics: invalid JSON" end
  return data, nil
end

local function number_of(name)
  local n = host.text.chapter_num(tostring(name or ""))
  if n == nil or n ~= n then return 0 end
  return n
end

local methods = {}

methods.search = function(params)
  local q = params.title or params.query or params.q or ""
  if q == "" then return { ok = false, error = "4ucomics: empty query" } end
  local d, err = get_json("/search/" .. host.url_encode(q) .. "?adult=1")
  if err then return { ok = false, error = err } end
  local out = {}
  for _, h in ipairs(d.hits or {}) do
    out[#out + 1] = {
      id = h.slug,
      name = h.title or "",
      cover = proxy_img(h.cover or ""),
      status = status_of(h.statuses),
    }
  end
  -- UI contract: {items, total} envelope, not a bare array (search_script run()).
  return { ok = true, data = { items = out, total = #out } }
end

methods.detail = function(params)
  local slug = params.id or ""
  if slug == "" then return { ok = false, error = "4ucomics: missing id" } end
  local d, err = get_json("/manga/" .. slug)
  if err then return { ok = false, error = err } end
  local m = d.manga or {}
  local authors = {}
  local function names_of(raw)
    if type(raw) == "string" then return { raw } end
    if type(raw) ~= "table" then return {} end
    local out = {}
    if raw[1] ~= nil then
      for _, a in ipairs(raw) do out[#out + 1] = tostring(a) end
      return out
    end
    for _, a in pairs(raw) do out[#out + 1] = tostring(a) end
    return out
  end
  for _, a in ipairs(names_of(m.authors)) do authors[#authors + 1] = a end
  -- the API carries no series start date; the earliest chapter date is the only
  -- real one available (and the sort order makes "first" = oldest here anyway).
  local first = ""
  for _, c in ipairs(m.chapters or {}) do
    local ds = tostring(c.date or ""):sub(1, 10)
    if ds ~= "" and (first == "" or ds < first) then first = ds end
  end
  return { ok = true, data = {
    id = m.slug or slug,
    name = m.title or "",
    cover = proxy_img(m.cover or ""),
    status = status_of(m.statuses),
    tags = m.genres or {},
    authors = authors,
    artists = {},
    description = m.description or "",
    release_at = (first ~= "") and first or 0,
    url = "https://4ucomics.com/manga/" .. (m.slug or slug),
  } }
end

methods.chapters = function(params)
  local slug = params.id or ""
  if slug == "" then return { ok = true, data = {} } end
  local d, err = get_json("/manga/" .. slug)
  if err then return { ok = false, error = err } end
  local m = d.manga or {}
  local out = {}
  for _, c in ipairs(m.chapters or {}) do
    local name = tostring(c.name or c.rawChapter or "")
    if name ~= "" then
      local dstr = tostring(c.date or "")
      out[#out + 1] = {
        id = slug .. ":" .. name,
        number = number_of(name),
        title = "",
        release_at = (dstr ~= "") and dstr:sub(1, 10) or 0,
        created_at = (dstr ~= "") and dstr:sub(1, 10) or 0,
        pages = 0,
      }
    end
  end
  return { ok = true, data = out }
end

methods.pages = function(params)
  local cid = params.id or ""
  local slug, sep, name = cid:match("^(.-)(:)(.+)$")
  if not sep then return { ok = false, error = "4ucomics: malformed chapter id " .. cid } end
  local d, err = get_json("/manga/" .. slug .. "?chapter=" .. host.url_encode(name))
  if err then return { ok = false, error = err } end
  local m = d.manga or {}
  for _, c in ipairs(m.chapters or {}) do
    if tostring(c.name or c.rawChapter or "") == name then
      local out = {}
      for _, p in ipairs(c.pages or {}) do
        local urls = p.pageURLs or {}
        if urls[1] then out[#out + 1] = proxy_img(urls[1]) end
      end
      return { ok = true, data = out }
    end
  end
  return { ok = false, error = "4ucomics: chapter not found " .. name }
end

function handle(req)
  local fn = methods[req.method]
  if fn == nil then return { ok = false, error = "unknown method " .. tostring(req.method) } end
  return fn(req.params or {})
end
