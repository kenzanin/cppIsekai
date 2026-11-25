-- Read Comics Online (readcomicsonline.ru): server-rendered Blade pages behind
-- Cloudflare. Search is /advanced-search?name=.. (all results, no pager),
-- detail/chapter lists are plain HTML, and a chapter page carries every page
-- image in <div id="reader-all">. Ported from the HAR capture in ./tmp.
local BASE = "https://readcomicsonline.ru"

-- Cloudflare challenges plain clients: the site needs a bound cf_clearance.
-- scripts/refresh-rco-cookie.py mints it into the gitignored config.json from
-- the daemon's own Chrome (which clears the JS challenge automatically).
-- Server-rendered HTML, no SPA at scrape time.
local UA = config.get("user_agent") or ""
local COOKIE = config.get("cookie") or ""
local OPTS = { timeout_ms = 60000, user_agent = UA }
if COOKIE ~= "" then
  OPTS.headers = { Cookie = COOKIE }
end

-- Category options are publishers, not story genres. Keep the site's own
-- name->id map so a UI tag selection can build the numeric ?category= filter.
local cat_map, cat_names

methods = {}

local function get(path)
  local body, err = host.http.fetch_body(BASE .. path, OPTS)
  if not body or body == "" then
    return nil, err
  end
  if body:find("Just a moment", 1, true) then
    return nil, "Cloudflare challenge — run scripts/refresh-rco-cookie.py"
  end
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

local function categories(html)
  if cat_map then
    return cat_map, cat_names
  end
  cat_map, cat_names = {}, {}
  local block = find1(html, [[(?s)<select name="category"[^>]*>(.*?)</select>]]) or ""
  for _, m in ipairs(all(block, [[<option value="([0-9]+)"[^>]*>([^<]+)</option>]])) do
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
    for name in tags:gmatch("[^,]+") do
      list[#list + 1] = clean(name)
    end
    tags = list
  end
  if type(tags) == "table" and #tags > 0 then
    local form = get("/advanced-search") or ""
    local map = categories(form)
    category = map[tags[1]] or map[tostring(tags[1]):lower()] or ""
  end
  return "/advanced-search?name=" .. host.url_encode(q) .. "&status_id=&type_id=&category=" .. host.url_encode(category)
end

local function parse_search(html)
  local items = {}
  local pat =
    [[(?s)<a href="https://readcomicsonline\.ru/comic/([^"]+)" class="group">.*?<div class="rc-cover"><img[^>]+src="([^"]+)".*?<p[^>]*>([^<]+)</p>]]
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

return {
  BASE = BASE,
  UA = UA,
  COOKIE = COOKIE,
  OPTS = OPTS,
  get = get,
  find1 = find1,
  all = all,
  clean = clean,
  epoch = epoch,
  categories = categories,
  search_url = search_url,
  parse_search = parse_search,
}
