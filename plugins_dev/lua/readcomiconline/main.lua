-- Read Comics Online (readcomicsonline.ru): server-rendered Blade pages behind
-- Cloudflare. Search is /advanced-search?name=.. (all results, no pager),
-- detail/chapter lists are plain HTML, and a chapter page carries every page
-- image in <div id="reader-all">. Ported from the HAR capture in ./tmp.
local BASE = "https://readcomicsonline.ru"

-- Cloudflare challenges plain clients; the HAR capture supplied the clearance
-- cookie + its exact UA. Both live in manifest params so they can be refreshed
-- without touching code. Every page is server-rendered, no SPA/browser needed.
local UA = host.plugin.param("user_agent") or "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/158.0.0.0 Safari/537.36"
local COOKIE = host.plugin.param("cookie") or ""
local OPTS = { timeout_ms = 60000, user_agent = UA }
if COOKIE ~= "" then OPTS.headers = { Cookie = COOKIE } end

local function get(path)
  local body, err = host.http.fetch_body(BASE .. path, OPTS)
  if not body or body == "" then return nil, err end
  return body
end

local function find1(s, pattern)
  return host.re2.capture(pattern, tostring(s or ""))
end

local function all(s, pattern)
  return host.re2.find_all(pattern, tostring(s or "")) or {}
end

local function clean(s)
  return host.text.trim(host.text.unescape(host.text.strip_html(tostring(s or ""))))
end

local function epoch(s)
  return host.text.date_epoch(clean(s)) or 0
end

-- Category options are publishers, not story genres. Keep the site's own
-- name->id map so a UI tag selection can build the numeric ?category= filter.
local cat_map, cat_names
local function categories(html)
  if cat_map then return cat_map, cat_names end
  cat_map, cat_names = {}, {}
  local block = find1(html, [[(?s)<select name="category"[^>]*>(.*?)</select>]]) or ""
  for _, m in ipairs(all(block, [[<option value="([0-9]+)"[^>]*>([^<]+)</option>]]) ) do
    local id, name = m[2], clean(m[3])
    if id ~= "" and name ~= "" then
      cat_map[name] = id
      cat_map[name:lower()] = id
      cat_names[#cat_names + 1] = name
    end
  end
  return cat_map, cat_names
end

local function search_url(q, tags)
  local category = ""
  if type(tags) == "string" then
    local list = {}
    for name in tags:gmatch("[^,]+") do list[#list + 1] = clean(name) end
    tags = list
  end
  if type(tags) == "table" and #tags > 0 then
    local form = get("/advanced-search") or ""
    local map = categories(form)
    category = map[tags[1]] or map[tostring(tags[1]):lower()] or ""
  end
  return "/advanced-search?name=" .. host.url_encode(q) ..
         "&status_id=&type_id=&category=" .. host.url_encode(category)
end

local function parse_search(html)
  local items = {}
  local pat = [[(?s)<a href="https://readcomicsonline\.ru/comic/([^"]+)" class="group">.*?<div class="rc-cover"><img[^>]+src="([^"]+)".*?<p[^>]*>([^<]+)</p>]]
  for _, m in ipairs(all(html, pat)) do
    local slug, cover, name = m[2], m[3], clean(m[4])
    if slug ~= "" and name ~= "" then
      items[#items + 1] = {
        id = slug,
        name = name,
        cover = host.text.unescape(cover),
        url = BASE .. "/comic/" .. slug,
      }
    end
  end
  return items
end

methods = {}

methods.search = function(params)
  local q = clean(params.title or params.query or params.q or "")
  if q == "" then return { ok = true, data = { items = {}, total = 0, pages = 1 } } end
  local html, err = get(search_url(q, params.tags or params.genres))
  if not html then return { ok = false, error = "readcomiconline: search failed" .. (err and (": " .. err) or "") } end
  local items = parse_search(html)
  return { ok = true, data = { items = items, total = #items, pages = 1 } }
end

methods.detail = function(params)
  local id = clean(params.id or params.slug or "")
  if id == "" then return { ok = true, data = {} } end
  local html, err = get("/comic/" .. host.url_encode(id))
  if not html then return { ok = false, error = "readcomiconline: detail failed" .. (err and (": " .. err) or "") } end

  local raw_name = find1(html, [[(?s)<h1[^>]*>([^<]+)</h1>]]) or ""
  local tags = {}
  for _, m in ipairs(all(html, [[<a href="https://readcomicsonline\.ru/comic-list/category/[^"]+"[^>]*>([^<]+)</a>]]) ) do
    local name = clean(m[2])
    if name ~= "" then tags[#tags + 1] = name end
  end
  local synopsis = find1(html, [[(?s)Synopsis</h3>.*?<p class="text-sm leading-relaxed text-slate-300">(.*?)</p>]]) or ""

  return {
    ok = true,
    data = {
      id = id,
      name = clean(raw_name),
      cover = host.text.unescape(find1(html, [[class="w-full rounded-xl[^"]*" src="([^"]+)"]]) or ""),
      status = host.text.normalize_status(find1(html, [[bg-emerald-500/20[^>]*>([^<]+)</span>]]) or ""),
      description = clean(synopsis),
      tags = tags,
      authors = {},
      release_at = 0,
      url = BASE .. "/comic/" .. id,
    },
  }
end

methods.chapters = function(params)
  local id = clean(params.id or params.slug or "")
  if id == "" then return { ok = true, data = {} } end
  local html, err = get("/comic/" .. host.url_encode(id))
  if not html then return { ok = false, error = "readcomiconline: chapters failed" .. (err and (": " .. err) or "") } end

  local pat = [[(?s)<a href="https://readcomicsonline\.ru/comic/]] .. id ..
              [[/([0-9]+(?:\.[0-9]+)?)"[^>]*>(.*?)</a>]]
  local out, seen = {}, {}
  for _, m in ipairs(all(html, pat)) do
    local num = tonumber(m[2])
    if num and not seen[num] then
      seen[num] = true
      local date = find1(m[3], [[text-xs text-slate-500">([^<]+)</span>]]) or ""
      out[#out + 1] = {
        id = id .. ":" .. tostring(num),
        number = num,
        title = string.format("Chapter %g", num),
        release_at = epoch(date),
        pages = 0,
      }
    end
  end
  table.sort(out, function(a, b) return a.number < b.number end)
  return { ok = true, data = out }
end

methods.pages = function(params)
  local id = clean(params.id or params.chapter_id or "")
  local slug, num = id:match("^([^:]+):([0-9]+%.?[0-9]*)$")
  if not slug then return { ok = true, data = {} } end
  local html, err = get("/comic/" .. host.url_encode(slug) .. "/" .. num)
  if not html then return { ok = false, error = "readcomiconline: pages failed" .. (err and (": " .. err) or "") } end
  local block = find1(html, [[(?s)id="reader-all"[^>]*>(.*?)</div>]]) or ""
  local out = {}
  for _, m in ipairs(all(block, [[<img[^>]+src="([^"]+)"]]) ) do
    local url = host.text.unescape(m[2])
    if url ~= "" then out[#out + 1] = url end
  end
  return { ok = true, data = out }
end

methods.genres = function()
  local form = get("/advanced-search") or ""
  local _, names = categories(form)
  return { ok = true, data = names }
end

function handle(req)
  local fn = methods[req.method]
  if not fn then return { ok = false, error = "readcomiconline: unknown method " .. tostring(req.method) } end
  local ok, res = pcall(fn, req.params or {})
  if not ok then return { ok = false, error = "readcomiconline: " .. tostring(res) } end
  return res
end
