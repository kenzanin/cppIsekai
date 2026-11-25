local c = require("common")
local BASE = c.BASE
local UA = c.UA
local LIMIT = c.LIMIT
local STATUS = c.STATUS
local get = c.get
local txt = c.txt
local cards = c.cards
return function(params)
  local id = params.id or ""
  if id == "" then
    return { ok = false, error = "weebcentral: missing id" }
  end
  local body, err = get("/series/" .. host.url_encode(id))
  if err then
    return { ok = false, error = err }
  end
  local title = body:match('<h1 class="hidden md:block text%-2xl font%-bold">(.-)</h1>')
    or body:match("<h1[^>]*>([^<]+)</h1>")
  local desc = body:match('<strong>Description</strong>%s*<p class="whitespace%-pre%-wrap break%-words">(.-)</p>')
  local status = body:match("<strong>Status:%s*</strong>%s*.-<a[^>]*>([^<]+)</a>")
    or body:match("<strong>Status:</strong>%s*.-<a[^>]*>([^<]+)</a>")
  local year = body:match("<strong>Released:%s*</strong>%s*<span>([^<]+)</span>")
  local type_ = body:match("<strong>Type:%s*</strong>%s*<a[^>]*>([^<]+)</a>")
  local authors, tags = {}, {}
  for name in body:gmatch('search%?author=[^"]*"[^>]*>([^<]+)</a>') do
    authors[#authors + 1] = txt(name)
  end
  for t in body:gmatch('included_tag=[^"]*"[^>]*>([^<]+)</a>') do
    tags[#tags + 1] = txt(t)
  end
  return {
    ok = true,
    data = {
      id = id,
      name = txt(title or id),
      cover = "https://temp.compsci88.com/cover/fallback/" .. id .. ".jpg",
      status = STATUS[string.lower(status or "")] or "unknown",
      tags = tags,
      authors = authors,
      artists = {},
      description = desc and txt(desc) or "",
      release_at = year and tonumber(year:match("%d%d%d%d")) and (year:match("%d%d%d%d") .. "-01-01") or 0,
      url = BASE .. "/series/" .. id,
    },
  }
end
