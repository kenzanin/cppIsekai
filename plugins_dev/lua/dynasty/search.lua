local c = require("common")
local BASE = c.BASE
local OPTS = c.OPTS
local http_req = c.http_req
local find1 = c.find1
local clean = c.clean
local first = c.first
local get_json = c.get_json
local json_url = c.json_url
local split_id = c.split_id
local tags_of = c.tags_of
return function(params)
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
