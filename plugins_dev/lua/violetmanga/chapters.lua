local c = require("common")
local BASE = c.BASE
local http_req = c.http_req
local sel1 = c.sel1
return function(params)
  local slug = params.id or ""
  if slug == "" then return { ok = true, data = {} } end
  local body, err = http_req(BASE .. "/comics/" .. slug .. "/")
  if err ~= nil then return { ok = false, error = err } end

  -- detail-page SSR list; locked (coin-gated) anchors carry no href and are
  -- skipped by the attribute query. ids stay site-relative paths.
  local out = {}
  for _, href in ipairs(host.html.attr(body, "#chapterlist a", "href")) do
    local id = tostring(href):match("violetmanga%.com(/[^%?#]*)") or href
    local num = tostring(href):match("-chapter%-(.-)/?$") or ""
    out[#out + 1] = {
      id = id,
      number = tonumber(num) or 0,
      title = num ~= "" and ("Chapter " .. num) or "",
      release_at = 0, -- list carries no per-chapter dates
      pages = 0,
    }
  end
  return { ok = true, data = out }
end
