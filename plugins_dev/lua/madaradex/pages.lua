local c = require("common")
local BASE = c.BASE
local AJAX = c.AJAX
local MANGA_PATH = c.MANGA_PATH
local auth_done = c.auth_done
local warm_auth = c.warm_auth
local http_req = c.http_req
local find1 = c.find1
local abs_url = c.abs_url
local manga_url = c.manga_url
local img_src = c.img_src
local genre_map = c.genre_map
return function(params)
  local id = tostring(params.id or "")
  local slug, cslug = id:match("^(.-):(.+)$")
  if not slug or slug == "" or not cslug or cslug == "" then return { ok = true, data = {} } end

  local url = manga_url(slug) .. host.url_encode(cslug) .. "/"
  local body, err = http_req(url)
  if err ~= nil then return { ok = false, error = err } end

  -- the paged reader carries one image; the list layout has them all
  if body:find('id="single-pager"', 1, true) then
    local listed, lerr = http_req(url .. "?style=list")
    if lerr == nil and listed and listed ~= "" then body = listed end
  end

  -- tag-at-a-time scan (host.re2.find_index has no start-offset argument).
  -- 2026-11: theme dropped per-img wp-manga-chapter-img classes; page images
  -- now live in div.reading-content as lazyload imgs (data-src + placeholder src).
  local block = find1(body, [[(?s)class="[^"]*reading-content[^"]*".*]]) or body
  local pages, scanned, pos = {}, 0, 1
  while true do
    local s = block:find("<img", pos, true)
    if not s then break end
    local e = block:find(">", s, true)
    if not e then break end
    local tag = block:sub(s, e)
    scanned = scanned + 1
    local src = img_src(tag)
    if src ~= "" then pages[#pages + 1] = src end -- cdn host 403s without Referer; host sends site origin
    pos = e + 1
  end

  -- parsed page but no reader markup: broken upstream chapter, name it
  if #pages == 0 and scanned > 0 then
    return { ok = false, error = "no_pages: " .. cslug }
  end
  return { ok = true, data = pages }
end
