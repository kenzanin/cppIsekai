local c = require("common")
local BASE = c.BASE
local UA = c.UA
local COOKIE = c.COOKIE
local OPTS = c.OPTS
local get = c.get
local find1 = c.find1
local all = c.all
local clean = c.clean
local epoch = c.epoch
local categories = c.categories
local search_url = c.search_url
local parse_search = c.parse_search
return function(params)
  local id = clean(params.id or params.slug or "")
  if id == "" then return { ok = true, data = {} } end
  local html, err = get("/comic/" .. host.url_encode(id))
  if not html then return { ok = false, error = "readcomiconline: detail failed" .. (err and (": " .. err) or "") } end

  local raw_name = find1(html, [[(?s)<h1[^>]*>([^<]+)</h1>]]) or ""
  local tags = {}
  for _, m in ipairs(all(html, [[<a href="https://readcomicsonline\.ru/comic-list/category/[^"]+"[^>]*>([^<]+)</a>]]) ) do
    local name = clean(m[2])
    if name ~= "" then tags[#tags + 1] = name end
  end
  local synopsis = find1(html, [[(?s)Synopsis</h3>.*?<p class="text-sm leading-relaxed text-slate-300">(.*?)</p>]]) or ""

  return {
    ok = true,
    data = {
      id = id,
      name = clean(raw_name),
      cover = host.text.unescape(find1(html, [[class="w-full rounded-xl[^"]*" src="([^"]+)"]]) or ""),
      status = host.text.normalize_status(find1(html, [[bg-emerald-500/20[^>]*>([^<]+)</span>]]) or ""),
      description = clean(synopsis),
      tags = tags,
      authors = {},
      release_at = 0,
      url = BASE .. "/comic/" .. id,
    },
  }
end
