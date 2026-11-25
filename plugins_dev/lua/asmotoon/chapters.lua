local c = require("common")
local BASE = c.BASE
local OPTS = c.OPTS
local http_req = c.http_req
local find1 = c.find1
local clean = c.clean
local first = c.first
local status_of = c.status_of
local series_url = c.series_url
return function(params)
  local slug = find1(params.id or "", [[^/([^/]+)]]) or tostring(params.id or "")
  local body, err = http_req(BASE .. "/?_chapters_html=1&series_slug=" .. host.url_encode(slug))
  if not body or body == "" then
    return { ok = false, error = err }
  end
  local ok, doc = pcall(json.decode, body)
  if not ok or type(doc) ~= "table" then
    return { ok = false, error = "asmodeus: bad chapters envelope" }
  end
  local html = doc.rows_html or ""
  local hrefs = host.html.attr(html, "a.chapter-row[href]", "href") or {}
  local titles = host.html.attr(html, "a.chapter-row[href]", "data-title") or {}
  local out = {}
  for i, href in ipairs(hrefs) do
    local num = host.text.chapter_num(titles[i] or href) or 0
    out[#out + 1] = {
      id = slug .. ":" .. find1(href, [[^/([^/]+)]]),
      number = num,
      title = clean(titles[i] or ("Chapter " .. tostring(num))),
      release_at = 0,
      pages = 0,
      url = BASE .. href,
    }
  end
  table.sort(out, function(a, b)
    return a.number < b.number
  end)
  return { ok = true, data = out }
end
