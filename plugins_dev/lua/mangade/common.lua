-- MangaDE (mangade.io): public JSON API at api.mangade.io, ported from
-- goIsekai's src/en/mangade plus tmp/mangade.har. Every reply is
-- {"success":true,"data":...}; no HTML scraping, no crypto, no cookies.
-- Search is page-based (20/site page); detail carries news_chapters, and a
-- chapter's images come from /chapters/<chapterId>/view (comic+slug also work).
local API = host.plugin.param("api") or "https://api.mangade.io/api"
local SITE = host.plugin.param("site_url") or "https://mangade.io"
local OPTS = {
  timeout_ms = 45000,
  accept = "application/json, text/plain, */*",
  headers = { Referer = SITE .. "/", Origin = SITE },
}

-- The host passes the numeric comic id; tolerate the site's /comic/<slug>-pid<id>
-- URL too, so a pasted link works as an id.

methods = {}

local genre_map

local function get_json(path)
  local body, err = host.http.fetch_body(API .. path, OPTS)
  if not body or body == "" then
    return nil, err
  end
  local ok, doc = pcall(json.decode, body)
  if not ok or type(doc) ~= "table" or doc.success ~= true or type(doc.data) ~= "table" then
    return nil, "mangade: bad envelope"
  end
  return doc.data
end

local function find1(s, pat)
  return host.re2.capture(pat, tostring(s or ""))
end

local function clean(s)
  return host.text.trim(host.text.unescape(tostring(s or "")))
end

local function epoch(s)
  return host.text.date_epoch(clean(s)) or 0
end

local function split_names(s)
  local out = {}
  for raw in tostring(s or ""):gmatch("[^,]+") do
    local name = clean(raw)
    if name ~= "" then
      out[#out + 1] = name
    end
  end
  return out
end

local function names(list)
  local out = {}
  if type(list) ~= "table" then
    return out
  end
  for _, v in ipairs(list) do
    local name = type(v) == "table" and (v.name or v.title or v.slug) or v
    name = clean(name)
    if name ~= "" then
      out[#out + 1] = name
    end
  end
  return out
end

local function comic_id(v)
  local s = tostring(v or "")
  return find1(s, "pid(%d+)") or s
end

local function genre_ids(tags)
  if type(tags) == "string" then
    local list = {}
    for name in tags:gmatch("[^,]+") do
      list[#list + 1] = clean(name)
    end
    tags = list
  end
  if type(tags) ~= "table" or #tags == 0 then
    return {}
  end
  if not genre_map then
    genre_map = {}
    local data = get_json("/genres?size=500")
    for _, g in ipairs((data and data.genres) or {}) do
      if g.id then
        genre_map[clean(g.name):lower()] = tostring(g.id)
        genre_map[clean(g.slug):lower()] = tostring(g.id)
      end
    end
  end
  local out = {}
  for _, tag in ipairs(tags) do
    local id = genre_map[clean(tag):lower()] or tostring(tag)
    if id ~= "" then
      out[#out + 1] = id
    end
  end
  return out
end

return {
  API = API,
  SITE = SITE,
  OPTS = OPTS,
  get_json = get_json,
  find1 = find1,
  clean = clean,
  epoch = epoch,
  split_names = split_names,
  names = names,
  comic_id = comic_id,
  genre_ids = genre_ids,
}
