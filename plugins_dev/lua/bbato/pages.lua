local c = require("common")
local BASE = c.BASE
local OPTS = c.OPTS
local XHR = c.XHR
local http_req = c.http_req
local find1 = c.find1
local clean = c.clean
local first = c.first
local manga_url = c.manga_url
local slug_of = c.slug_of
local parse_cards = c.parse_cards
return function(params)
  local composite = tostring(params.id or "")
  local slug, chapter = composite:match("^([^:]+):(.+)$")
  if not slug or not chapter then
    return { ok = true, data = {} }
  end
  local body, err = http_req(BASE .. "/read/" .. host.url_encode(slug) .. "/" .. host.url_encode(chapter))
  if not body or body == "" then
    return { ok = false, error = err }
  end
  -- Page images carry data-number; only the first ones ship a real src and the
  -- rest are lazy (inline SVG placeholder in src, real URL in data-src). The
  -- "do not repost" notice image (data-number='notice') rides along under
  -- /assets/images/, so read attr in order and drop placeholders/notice.
  local function usable(u)
    return u ~= "" and not u:match("^data:") and not u:find("/assets/images/", 1, true)
  end
  local urls = {}
  for _, attr in ipairs({ "data-fallback", "data-src", "src" }) do
    local list = {}
    for _, u in ipairs(host.html.attr(body, ".pages img[data-number]", attr) or {}) do
      u = clean(u)
      if usable(u) then
        list[#list + 1] = host.url_abs(BASE, u)
      end
    end
    if #list > 0 then
      urls = list
      break
    end
  end
  local out = {}
  for _, u in ipairs(urls) do
    if u ~= "" and not u:find("notice", 1, true) then
      out[#out + 1] = u
    end
  end
  return { ok = true, data = out }
end
