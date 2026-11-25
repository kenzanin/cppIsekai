-- Asmodeus Scans (asmotoon.com): KeyoappV2 white-label, HTML pages plus two
-- AJAX helpers. Port of goIsekai extension-source/src/en/asmotoon (Asmotoon.kt
-- over lib-multisrc/keyoapp/KeyoappV2.kt). Search = /?_ajax=search&q=&offset=;
-- chapter rows = /?_chapters_html=1&series_slug=; pages = #strip-reader imgs.
local BASE = host.plugin.param("site_url") or "https://asmotoon.com"
local OPTS = {
  timeout_ms = 45000,
  headers = {
    ["User-Agent"] = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/158.0.0.0 Safari/537.36",
    ["Referer"] = BASE .. "/",
    ["X-Requested-With"] = "XMLHttpRequest",
  },
}

local function http_req(url, extra)
  if not extra then
    return host.http.fetch_body(url, OPTS)
  end
  local o = {}
  for k, v in pairs(OPTS) do
    o[k] = v
  end
  for k, v in pairs(extra) do
    o[k] = v
  end
  return host.http.fetch_body(url, o)
end

local function find1(s, pat)
  return host.re2.capture(pat, tostring(s or ""))
end
local function clean(s)
  return host.text.trim(host.text.unescape(host.text.strip_html(tostring(s or ""))))
end
local function first(t)
  return type(t) == "table" and t[1] or nil
end
local function status_of(text)
  local t = tostring(text or ""):lower()
  if t:find("ongoing", 1, true) then
    return "ongoing"
  end
  if t:find("completed", 1, true) then
    return "completed"
  end
  if t:find("dropped", 1, true) or t:find("cancelled", 1, true) then
    return "cancelled"
  end
  if t:find("paused", 1, true) or t:find("hiatus", 1, true) then
    return "hiatus"
  end
  return ""
end

local function clean(s)
  return host.text.trim(host.text.unescape(host.text.strip_html(tostring(s or ""))))
end
local function first(t)
  return type(t) == "table" and t[1] or nil
end
local function status_of(text)
  local t = tostring(text or ""):lower()
  if t:find("ongoing", 1, true) then
    return "ongoing"
  end
  if t:find("completed", 1, true) then
    return "completed"
  end
  if t:find("dropped", 1, true) or t:find("cancelled", 1, true) then
    return "cancelled"
  end
  if t:find("paused", 1, true) or t:find("hiatus", 1, true) then
    return "hiatus"
  end
  return ""
end

local function first(t)
  return type(t) == "table" and t[1] or nil
end
local function status_of(text)
  local t = tostring(text or ""):lower()
  if t:find("ongoing", 1, true) then
    return "ongoing"
  end
  if t:find("completed", 1, true) then
    return "completed"
  end
  if t:find("dropped", 1, true) or t:find("cancelled", 1, true) then
    return "cancelled"
  end
  if t:find("paused", 1, true) or t:find("hiatus", 1, true) then
    return "hiatus"
  end
  return ""
end

local function status_of(text)
  local t = tostring(text or ""):lower()
  if t:find("ongoing", 1, true) then
    return "ongoing"
  end
  if t:find("completed", 1, true) then
    return "completed"
  end
  if t:find("dropped", 1, true) or t:find("cancelled", 1, true) then
    return "cancelled"
  end
  if t:find("paused", 1, true) or t:find("hiatus", 1, true) then
    return "hiatus"
  end
  return ""
end

local function series_url(slug)
  return BASE .. "/" .. host.url_encode(slug)
end

methods = {}

methods.search = function(params)
  local q = clean(params.title or params.query or params.q or "")
  local page = math.max(1, tonumber(params.page) or 1)
  local limit = 20
  local url = BASE .. "/?_ajax=search&q=" .. host.url_encode(q) .. "&offset=" .. ((page - 1) * limit)
  local body, err = http_req(url)
  if not body or body == "" then
    return { ok = false, error = err }
  end
  local ok, doc = pcall(json.decode, body)
  if not ok or type(doc) ~= "table" then
    return { ok = false, error = "asmodeus: bad search envelope" }
  end
  local html = doc.html or ""
  local hrefs = host.html.attr(html, "a.group[href]", "href") or {}
  local names = host.html.select(html, "a.group[href] h2") or {}
  local covers = host.html.attr(html, "a.group[href] img", "data-src") or {}
  local items = {}
  for i, href in ipairs(hrefs) do
    local slug = find1(href, [[^/([^/]+)]])
    if slug and slug ~= "" then
      items[#items + 1] = {
        id = slug,
        name = clean(names[i] or ""),
        cover = covers[i] or "",
        url = BASE .. href,
      }
    end
  end
  return {
    ok = true,
    data = {
      items = items,
      total = #items + (doc.has_more and limit or 0),
      pages = page + (doc.has_more and 1 or 0),
    },
  }
end

return {
  BASE = BASE,
  OPTS = OPTS,
  http_req = http_req,
  find1 = find1,
  clean = clean,
  first = first,
  status_of = status_of,
  series_url = series_url,
}
