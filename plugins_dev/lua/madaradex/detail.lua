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
  local slug = host.text.trim(tostring(params.id or ""))
  if slug == "" then return { ok = true, data = {} } end
  local body, err = http_req(manga_url(slug))
  if err ~= nil then return { ok = false, error = err } end

  local tblock = find1(body, [[(?s)class="post-title[^"]*">(.*?)</div>]]) or ""
  local name = host.text.unescape(host.text.strip_html(find1(tblock, [[(?s)<h[1-6][^>]*>(.*?)</h[1-6]>]]) or ""))
  local author = host.text.unescape(host.text.strip_html(find1(body, [[(?s)class="author-content">(.*?)</div>]]) or ""))
  local cover = img_src(find1(body, [[(?s)class="summary_image".*?</a>]]) or "")

  local desc = find1(body, [[(?s)class="summary__content[^"]*">(.*?)<span\s+class="[^"]*content-readmore"]]) or ""
  if desc == "" then
    desc = find1(body, [[(?s)class="summary__content[^"]*">(.*?)</div>]]) or ""
  end
  desc = host.text.unescape(host.text.trim(host.text.strip_html(desc))):gsub("%s+", " ")

  local tags = {}
  for _, g in ipairs(host.re2.find_all([[<a[^>]*>([^<]+)</a>]], find1(body, [[(?s)class="genres-content">(.*?)</div>]]))) do
    local label = host.text.unescape(host.text.trim(g[2] or ""))
    if label ~= "" then tags[#tags + 1] = label end
  end

  local status = ""
  local sblock = find1(body, [[(?s)Status\s*</h5>.*?class="summary-content">\s*([^<]+)]])
  if sblock then status = host.text.normalize_status(host.text.trim(sblock)) end

  return { ok = true, data = {
    id = slug,
    name = name,
    cover = cover,
    status = status,
    tags = tags,
    release_at = 0, -- Madara dates are human strings; no machine date here
    description = desc,
    authors = author ~= "" and { author } or {},
    url = manga_url(slug),
  } }
end
