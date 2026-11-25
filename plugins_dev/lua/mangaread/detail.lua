local c = require("common")
local BASE = c.BASE
local GENRE_PATH = c.GENRE_PATH
local http_req = c.http_req
local find1 = c.find1
return function(params)
  local slug = params.id or ""
  local empty = {
    id = slug,
    name = "",
    cover = "",
    status = "unknown",
    tags = {},
    release_at = 0,
    description = "",
    authors = {},
    artists = {},
    url = BASE .. "/manga/" .. slug .. "/",
  }
  if slug == "" then
    return { ok = true, data = empty }
  end

  local body, err = http_req(BASE .. "/manga/" .. slug .. "/")
  if err ~= nil then
    return { ok = false, error = err }
  end

  local cover_block = find1(body, [[(?s)class="summary_image".*?</a>]]) or ""
  local cover =
    host.text.trim(find1(cover_block, [[data-src="([^"]+)"]]) or find1(cover_block, [[src="([^"]+)"]]) or "")

  local tblock = find1(body, [[(?s)class="post-title[^"]*">(.*?)</div>]]) or ""
  local title = host.text.unescape(host.text.trim(find1(tblock, [[(?s)<h[1-6][^>]*>(.*?)</h[1-6]>]]) or ""))

  local ablock = find1(body, [[(?s)class="author-content">(.*?)</div>]]) or ""
  local author = host.text.strip_html(ablock):gsub(",", ", ")

  local gblock = find1(body, [[(?s)class="genres-content">(.*?)</div>]]) or ""
  local tags = {}
  for _, g in ipairs(host.re2.find_all([[<a[^>]*>([^<]+)</a>]], gblock)) do
    local name = host.text.unescape(host.text.trim(g[2] or ""))
    if name ~= "" then
      tags[#tags + 1] = name
    end
  end

  local status = "unknown"
  local sblock = find1(body, [[(?s)class="post-status">(.*?)$]]) or ""
  if sblock ~= "" then
    local after = find1(sblock, [[(?s)Status\s*</h5>.*?class="summary-content">\s*([^<]+)]])
    if after then
      status = host.text.normalize_status(after)
    end
  end

  local dblock = find1(body, [[(?s)class="summary__content[^"]*">(.*?)<span\s+class="[^"]*content-readmore"]]) or ""
  if dblock == "" then
    dblock = find1(body, [[(?s)class="summary__content[^"]*">(.*?)</div>]]) or ""
  end
  local description = host.text.strip_html(dblock):gsub("%s+", " ")

  local data = empty
  data.name = title
  data.cover = cover
  data.status = status
  data.tags = tags
  data.description = description
  if author ~= "" then
    data.authors = { author }
  end
  return { ok = true, data = data }
end
