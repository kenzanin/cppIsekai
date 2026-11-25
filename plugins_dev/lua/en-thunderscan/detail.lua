local c = require("common")
local BASE = c.BASE
local trim = c.trim
local cap = c.cap
local http_body = c.http_body
local fetch_detail_body = c.fetch_detail_body
return function(params)
  local slug = tostring(params.id or "")
  local body = fetch_detail_body(slug)
  if not body then
    return { ok = true, data = { id = slug } }
  end

  local name = host.text.unescape(trim(cap([[(?s)<h1 class="entry-title"[^>]*>(.*?)</h1>]], body) or ""))

  local thumb = cap([[(?s)<div class="thumb"[^>]*>(.*?)</div>]], body) or ""
  local cover = cap([[data-src="(https?://[^"]+)"]], thumb) or cap([[src="(https?://[^"]+)"]], thumb) or ""

  local author =
    host.text.unescape(trim(cap([[(?s)<div class="imptdt">\s*<h1>\s*Author\s*</h1>\s*<i>([^<]+)</i>]], body) or ""))

  local genres = {}
  local gblock = cap([[(?s)<span class="mgen">(.*?)</span>]], body) or ""
  for _, m in ipairs(host.re2.find_all([[<a[^>]*>([^<]+)</a>]], gblock)) do
    local fname = host.text.unescape(trim(m[2]))
    if fname ~= "" then
      genres[#genres + 1] = fname
    end
  end

  local status = trim(cap([[(?s)<div class="imptdt">\s*<h1>\s*Status\s*</h1>\s*<i>([^<]+)</i>]], body) or "")

  local dblock = cap([[(?s)entry-content entry-content-single"[^>]*>(.*?)</div>]], body) or ""
  local description = host.re2.replace([[\s+]], " ", host.text.strip_html(dblock))

  return {
    ok = true,
    data = {
      id = slug,
      name = name,
      cover = trim(cover),
      status = host.text.normalize_status(status),
      tags = genres,
      -- theme renders no machine date on the series page
      release_at = 0,
      description = description,
      authors = author ~= "" and { author } or {},
      artists = {},
      url = BASE .. "/comics/" .. slug .. "/",
    },
  }
end
