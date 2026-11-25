-- MangaK (mangak.io): Next.js front end over a public JSON API at
-- api.mangak.io. Ported from goIsekai's wasm plugin plus tmp/mangak.har;
-- the HAR confirmed the live shapes (pagination, genres, chapter images).
-- Every response is {"success":true,"data":...}; no HTML scraping, no crypto.
local API = host.plugin.param("api") or "https://api.mangak.io"
local SITE = host.plugin.param("site_url") or "https://mangak.io"
local OPTS = {
  timeout_ms = 45000,
  accept = "application/json, text/plain, */*",
  headers = { Referer = SITE .. "/" },
}





local genre_map


methods = {}






local function get_json(path)
  local body, err = host.http.fetch_body(API .. path, OPTS)
  if not body or body == "" then return nil, err end
  local ok, doc = pcall(json.decode, body)
  if not ok or type(doc) ~= "table" or doc.success ~= true then
    return nil, "mangak: bad envelope"
  end
  return doc.data
end

local function find1(s, pat)
  return host.re2.capture(pat, tostring(s or ""))
end

local function names(list)
  local out = {}
  if type(list) ~= "table" then return out end
  for _, v in ipairs(list) do
    local name = type(v) == "table" and (v.name or v.title) or v
    if type(name) == "string" and name ~= "" then out[#out + 1] = host.text.unescape(name) end
  end
  return out
end

local function epoch(v)
  return host.text.date_epoch(tostring(v or "")) or 0
end

local function ensure_genres()
  if genre_map then return genre_map end
  genre_map = {}
  local data = get_json("/genres")
  if data and type(data.items) == "table" then
    for _, g in ipairs(data.items) do
      if type(g) == "table" and g.name then
        genre_map[host.text.trim(g.name)] = g.slug or g.name
        genre_map[tostring(g.name):lower()] = g.slug or g.name
      end
    end
  end
  return genre_map
end

local function slug_of(id)
  id = tostring(id or "")
  return find1(id, [[/([^/?#]+)]]) or id
end

return { API=API, SITE=SITE, OPTS=OPTS, get_json=get_json, find1=find1, names=names, epoch=epoch, ensure_genres=ensure_genres, slug_of=slug_of }
