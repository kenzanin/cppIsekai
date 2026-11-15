-- 4ucomics: private JSON API v1 behind Cloudflare (the HTML shell is a JS
-- SPA). API calls ride the lightpanda renderer profile -- the challenge
-- solver (decisions/renderer-fetch-engines); page/cover CDNs are open, the
-- host image path (plain curl + manifest image_referer) reaches them direct.
-- Envelope { ok, data | error }; canonical manga fields per the mangafire plugin.
local API = "https://4ucomics.com/api/v1"

-- Site vocab (anilist-style) -> canonical; unknown falls to normalize_status.
local STATUS = {
  RELEASING = "Ongoing", ONGOING = "Ongoing",
  FINISHED = "Completed", COMPLETED = "Completed",
  HIATUS = "Hiatus",
  CANCELLED = "Dropped", DROPPED = "Dropped",
  NOT_YET_RELEASED = "Upcoming", UPCOMING = "Upcoming",
}

local function status_of(list)
  for _, s in ipairs(list or {}) do
    local m = STATUS[string.upper(tostring(s))]
    if m then return m end
  end
  return "Unknown"
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
      title = h.title or "",
      cover = h.cover or "",
      status = "",
      author = "",
      genres = {},
      description = "",
    }
  end
  return { ok = true, data = out }
end

methods.detail = function(params)
  local slug = params.id or ""
  if slug == "" then return { ok = false, error = "4ucomics: missing id" } end
  local d, err = get_json("/manga/" .. slug)
  if err then return { ok = false, error = err } end
  local m = d.manga or {}
  local authors = {}
  for _, a in ipairs(m.authors or {}) do authors[#authors + 1] = tostring(a) end
  return { ok = true, data = {
    id = m.slug or slug,
    title = m.title or "",
    cover = m.cover or "",
    status = status_of(m.statuses),
    author = table.concat(authors, ", "),
    artist = "",
    genres = m.genres or {},
    description = m.description or "",
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
      out[#out + 1] = { id = slug .. ":" .. name, number = number_of(name), title = "Chapter " .. name }
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
        if urls[1] then out[#out + 1] = urls[1] end
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
