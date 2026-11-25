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






methods = {}






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

local function clean(s) return host.text.trim(host.text.unescape(host.text.strip_html(tostring(s or "")))) end
local function first(t) return type(t) == "table" and t[1] or nil end

local function get_json(path)
  local body, err = http_req(path)
  if not body or body == "" then return nil, err end
  local ok, doc = pcall(json.decode, body)
  if not ok or type(doc) ~= "table" then return nil, "dynasty: bad json" end
  return doc
end

local function first(t) return type(t) == "table" and t[1] or nil end

local function get_json(path)
  local body, err = http_req(path)
  if not body or body == "" then return nil, err end
  local ok, doc = pcall(json.decode, body)
  if not ok or type(doc) ~= "table" then return nil, "dynasty: bad json" end
  return doc
end

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

return { BASE=BASE, OPTS=OPTS, http_req=http_req, find1=find1, clean=clean, first=first, get_json=get_json, json_url=json_url, split_id=split_id, tags_of=tags_of }
