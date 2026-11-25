-- Bbato (bato1.co.uk): HTML cards + JSON chapter list, port of goIsekai
-- extension-source/src/en/bbato (Bbato.kt). Search = /filter?keyword=,
-- detail = /manga/<slug>, chapters = /get-chapter-list?slug=<slug> (XHR),
-- pages = .pages img[data-number] on /read/<slug>/<chapter>.
local BASE = host.plugin.param("site_url") or "https://bato1.co.uk"
local OPTS = {
  timeout_ms = 45000,
  headers = {
    ["User-Agent"] = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/158.0.0.0 Safari/537.36",
    ["Referer"] = BASE .. "/",
  },
}
local XHR = { ["X-Requested-With"] = "XMLHttpRequest" }



methods = {}






local function http_req(url, extra)
  if not extra then return host.http.fetch_body(url, OPTS) end
  local o = {}
  for k, v in pairs(OPTS) do o[k] = v end
  for k, v in pairs(extra) do o[k] = v end
  return host.http.fetch_body(url, o)
end

local function find1(s, pat) return host.re2.capture(pat, tostring(s or "")) end
local function clean(s) return host.text.trim(host.text.unescape(host.text.strip_html(tostring(s or "")))) end
local function first(t) return type(t) == "table" and t[1] or nil end
local function manga_url(slug) return BASE .. "/manga/" .. host.url_encode(slug) end
local function slug_of(id) return find1(id, [[/manga/([^/?#]+)]]) or tostring(id or "") end

local function parse_cards(body)
  local hrefs = host.html.attr(body, ".mt-updates-grid .mt-card a.mt-card-cover", "href") or {}
  local names = host.html.select(body, ".mt-updates-grid .mt-card .mt-card-title") or {}
  local covers = host.html.attr(body, ".mt-updates-grid .mt-card a.mt-card-cover img", "data-src") or {}
  if #covers == 0 then covers = host.html.attr(body, ".mt-updates-grid .mt-card a.mt-card-cover img", "src") or {} end
  local out = {}
  for i, href in ipairs(hrefs) do
    local slug = find1(href, [[/manga/([^/?#]+)]])
    if slug then
      out[#out + 1] = { id = slug, name = clean(names[i] or ""), cover = covers[i] or "", url = href }
    end
  end
  return out
end

local function clean(s) return host.text.trim(host.text.unescape(host.text.strip_html(tostring(s or "")))) end
local function first(t) return type(t) == "table" and t[1] or nil end
local function manga_url(slug) return BASE .. "/manga/" .. host.url_encode(slug) end
local function slug_of(id) return find1(id, [[/manga/([^/?#]+)]]) or tostring(id or "") end

local function parse_cards(body)
  local hrefs = host.html.attr(body, ".mt-updates-grid .mt-card a.mt-card-cover", "href") or {}
  local names = host.html.select(body, ".mt-updates-grid .mt-card .mt-card-title") or {}
  local covers = host.html.attr(body, ".mt-updates-grid .mt-card a.mt-card-cover img", "data-src") or {}
  if #covers == 0 then covers = host.html.attr(body, ".mt-updates-grid .mt-card a.mt-card-cover img", "src") or {} end
  local out = {}
  for i, href in ipairs(hrefs) do
    local slug = find1(href, [[/manga/([^/?#]+)]])
    if slug then
      out[#out + 1] = { id = slug, name = clean(names[i] or ""), cover = covers[i] or "", url = href }
    end
  end
  return out
end

local function first(t) return type(t) == "table" and t[1] or nil end
local function manga_url(slug) return BASE .. "/manga/" .. host.url_encode(slug) end
local function slug_of(id) return find1(id, [[/manga/([^/?#]+)]]) or tostring(id or "") end

local function parse_cards(body)
  local hrefs = host.html.attr(body, ".mt-updates-grid .mt-card a.mt-card-cover", "href") or {}
  local names = host.html.select(body, ".mt-updates-grid .mt-card .mt-card-title") or {}
  local covers = host.html.attr(body, ".mt-updates-grid .mt-card a.mt-card-cover img", "data-src") or {}
  if #covers == 0 then covers = host.html.attr(body, ".mt-updates-grid .mt-card a.mt-card-cover img", "src") or {} end
  local out = {}
  for i, href in ipairs(hrefs) do
    local slug = find1(href, [[/manga/([^/?#]+)]])
    if slug then
      out[#out + 1] = { id = slug, name = clean(names[i] or ""), cover = covers[i] or "", url = href }
    end
  end
  return out
end

local function manga_url(slug) return BASE .. "/manga/" .. host.url_encode(slug) end
local function slug_of(id) return find1(id, [[/manga/([^/?#]+)]]) or tostring(id or "") end

local function parse_cards(body)
  local hrefs = host.html.attr(body, ".mt-updates-grid .mt-card a.mt-card-cover", "href") or {}
  local names = host.html.select(body, ".mt-updates-grid .mt-card .mt-card-title") or {}
  local covers = host.html.attr(body, ".mt-updates-grid .mt-card a.mt-card-cover img", "data-src") or {}
  if #covers == 0 then covers = host.html.attr(body, ".mt-updates-grid .mt-card a.mt-card-cover img", "src") or {} end
  local out = {}
  for i, href in ipairs(hrefs) do
    local slug = find1(href, [[/manga/([^/?#]+)]])
    if slug then
      out[#out + 1] = { id = slug, name = clean(names[i] or ""), cover = covers[i] or "", url = href }
    end
  end
  return out
end

local function slug_of(id) return find1(id, [[/manga/([^/?#]+)]]) or tostring(id or "") end

local function parse_cards(body)
  local hrefs = host.html.attr(body, ".mt-updates-grid .mt-card a.mt-card-cover", "href") or {}
  local names = host.html.select(body, ".mt-updates-grid .mt-card .mt-card-title") or {}
  local covers = host.html.attr(body, ".mt-updates-grid .mt-card a.mt-card-cover img", "data-src") or {}
  if #covers == 0 then covers = host.html.attr(body, ".mt-updates-grid .mt-card a.mt-card-cover img", "src") or {} end
  local out = {}
  for i, href in ipairs(hrefs) do
    local slug = find1(href, [[/manga/([^/?#]+)]])
    if slug then
      out[#out + 1] = { id = slug, name = clean(names[i] or ""), cover = covers[i] or "", url = href }
    end
  end
  return out
end

local function parse_cards(body)
  local hrefs = host.html.attr(body, ".mt-updates-grid .mt-card a.mt-card-cover", "href") or {}
  local names = host.html.select(body, ".mt-updates-grid .mt-card .mt-card-title") or {}
  local covers = host.html.attr(body, ".mt-updates-grid .mt-card a.mt-card-cover img", "data-src") or {}
  if #covers == 0 then covers = host.html.attr(body, ".mt-updates-grid .mt-card a.mt-card-cover img", "src") or {} end
  local out = {}
  for i, href in ipairs(hrefs) do
    local slug = find1(href, [[/manga/([^/?#]+)]])
    if slug then
      out[#out + 1] = { id = slug, name = clean(names[i] or ""), cover = covers[i] or "", url = href }
    end
  end
  return out
end

return { BASE=BASE, OPTS=OPTS, XHR=XHR, http_req=http_req, find1=find1, clean=clean, first=first, manga_url=manga_url, slug_of=slug_of, parse_cards=parse_cards }
