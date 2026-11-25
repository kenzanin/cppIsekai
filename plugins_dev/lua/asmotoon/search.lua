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
  local q = clean(params.title or params.query or params.q or "")
  local page = math.max(1, tonumber(params.page) or 1)
  local limit = 20
  local url = BASE .. "/?_ajax=search&q=" .. host.url_encode(q) .. "&offset=" .. ((page - 1) * limit)
  local body, err = http_req(url)
  if not body or body == "" then return { ok = false, error = err } end
  local ok, doc = pcall(json.decode, body)
  if not ok or type(doc) ~= "table" then return { ok = false, error = "asmodeus: bad search envelope" } end
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
  return { ok = true, data = { items = items, total = #items + (doc.has_more and limit or 0),
                               pages = page + (doc.has_more and 1 or 0) } }
end
