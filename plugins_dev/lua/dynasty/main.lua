-- Dynasty Scans: search is HTML (/search?q=...&classes[]=...), while detail,
-- chapters and pages come from per-entry JSON at /<directory>/<permalink>.json.
-- Port of goIsekai extension-source/src/en/dynasty (Dynasty.kt + Dto.kt).
local BASE = host.plugin.param("site_url") or "https://dynasty-scans.com"
local OPTS = {
  timeout_ms = 45000,
  headers = {
    ["User-Agent"] = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/158.0.0.0 Safari/537.36",
    ["Referer"] = BASE .. "/",
    ["Accept"] = "application/json, text/html, */*",
  },
}

local function http_req(url)
  return host.http.fetch_body(BASE .. url, OPTS)
end

local function find1(s, pat) return host.re2.capture(pat, tostring(s or "")) end
local function clean(s) return host.text.trim(host.text.unescape(host.text.strip_html(tostring(s or "")))) end
local function first(t) return type(t) == "table" and t[1] or nil end

local function get_json(path)
  local body, err = http_req(path)
  if not body or body == "" then return nil, err end
  local ok, doc = pcall(json.decode, body)
  if not ok or type(doc) ~= "table" then return nil, "dynasty: bad json" end
  return doc
end

local function json_url(dir, permalink)
  return "/" .. host.url_encode(dir) .. "/" .. host.url_encode(permalink) .. ".json"
end

local function split_id(id)
  id = tostring(id or "")
  return id:match("^([^:]+):(.+)$")
end

local function tags_of(list, kind)
  local out = {}
  for _, t in ipairs(list or {}) do
    if t.type == kind and t.name then out[#out + 1] = clean(t.name) end
  end
  return out
end

methods = {}

methods.search = function(params)
  local q = clean(params.title or params.query or params.q or "")
  local page = math.max(1, tonumber(params.page) or 1)
  local path = "/search?q=" .. host.url_encode(q) ..
               "&sort=best&classes%5B%5D=Series&classes%5B%5D=Chapter&classes%5B%5D=Doujin"
  if page > 1 then path = path .. "&page=" .. page end
  local body, err = http_req(path)
  if not body or body == "" then return { ok = false, error = err } end
  local hrefs = host.html.attr(body, ".chapter-list a.name[href]", "href") or {}
  local names = host.html.select(body, ".chapter-list a.name[href]") or {}
  local items, seen = {}, {}
  for i, href in ipairs(hrefs) do
    local dir, permalink = href:match("^/(%w+)/([^/?#]+)")
    if dir and permalink and not seen[href] then
      seen[href] = true
      items[#items + 1] = {
        id = dir .. ":" .. permalink,
        name = clean(names[i] or permalink:gsub("_", " ")),
        cover = "",
        url = BASE .. href,
      }
    end
  end
  local has_next = #host.html.attr(body, ".pagination a[rel=next]", "href") > 0
  return { ok = true, data = { items = items, pages = page + (has_next and 1 or 0) } }
end

methods.detail = function(params)
  local dir, permalink = split_id(params.id)
  if not dir or not permalink then return { ok = true, data = {} } end
  local doc, err = get_json(json_url(dir, permalink))
  if not doc then return { ok = false, error = err } end
  local status = table.concat(tags_of(doc.tags, "Status"), ", ")
  return { ok = true, data = {
    id = dir .. ":" .. permalink,
    name = clean(doc.name or ""),
    cover = doc.cover and host.url_abs(BASE, clean(doc.cover)) or "",
    description = clean(doc.description or ""),
    status = host.text.normalize_status(status),
    tags = tags_of(doc.tags, "General"),
    authors = tags_of(doc.tags, "Author"),
    artists = {},
    alt_titles = doc.aliases or {},
    url = doc.link or (BASE .. "/" .. dir .. "/" .. permalink),
  } }
end

methods.chapters = function(params)
  local dir, permalink = split_id(params.id)
  if not dir or not permalink then return { ok = true, data = {} } end
  local doc, err = get_json(json_url(dir, permalink))
  if not doc then return { ok = false, error = err } end
  if dir == "chapters" then
    return { ok = true, data = { { id = dir .. ":" .. permalink, number = 0,
                                   title = clean(doc.title or "Chapter"), release_at = host.text.date_epoch(tostring(doc.released_on or "")) or 0,
                                   pages = #(doc.pages or {}) } } }
  end
  local out, header = {}, ""
  for _, item in ipairs(doc.taggings or {}) do
    if item.header then
      header = clean(item.header)
    elseif item.permalink then
      local title = clean(item.title or "Chapter")
      if header ~= "" then title = header .. " " .. title end
      out[#out + 1] = {
        id = "chapters:" .. item.permalink,
        number = host.text.chapter_num(title) or 0,
        title = title,
        release_at = host.text.date_epoch(tostring(item.released_on or "")) or 0,
        pages = 0,
      }
    end
  end
  table.sort(out, function(a, b) return a.number < b.number end)
  return { ok = true, data = out }
end

methods.pages = function(params)
  local dir, permalink = split_id(params.id)
  if not permalink then
    permalink = tostring(params.id or "")
    dir = "chapters"
  end
  local doc, err = get_json(json_url(dir, permalink))
  if not doc then return { ok = false, error = err } end
  local out = {}
  for _, p in ipairs(doc.pages or {}) do
    if p.url and p.url ~= "" then out[#out + 1] = host.url_abs(BASE, clean(p.url)) end
  end
  return { ok = true, data = out }
end

methods.genres = function()
  local body = http_req("/search?q=") or ""
  local out, seen = {}, {}
  for _, href in ipairs(host.html.attr(body, "a[href^='/tags/']", "href") or {}) do
    local slug = find1(href, [[^/tags/([^/?#]+)]])
    if slug and not seen[slug] then
      seen[slug] = true
      out[#out + 1] = clean(slug:gsub("_", " "))
    end
  end
  return { ok = true, data = out }
end

function handle(req)
  local fn = methods[req.method]
  if not fn then return { ok = false, error = "dynasty: unknown method " .. tostring(req.method) } end
  local ok, res = pcall(fn, req.params or {})
  if not ok then return { ok = false, error = "dynasty: " .. tostring(res) } end
  return res
end
