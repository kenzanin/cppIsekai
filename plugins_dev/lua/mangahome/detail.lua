local c = require("common")
local BASE = c.BASE
local genres = c.genres
local BROWSER_UA = c.BROWSER_UA
local http_body = c.http_body
local date_epoch = c.date_epoch
return function(params)
  local id = tostring(params.id or "")
  if id == "" then return { ok = true, data = {} } end
  local body = http_body(BASE .. "/manga/" .. id)
  if not body or body == "" then return { ok = false, error = "upstream_unavailable" } end

  -- "<name> Manga - Read ... For Free!" -> strip the suffixes
  local raw = body:match("<title[^>]*>([^<]*)</title>") or id
  raw = raw:gsub("Manga%s*-%s*Read.*$", "")
  raw = raw:gsub("Manga%s*$", "")
  local title = host.text.trim(host.text.unescape(raw))

  -- the manga's own cover carries class="detail-cover"; src order varies, and
  -- the "latest/recommended" cards share ocover.jpg, so try the pairing first
  local cover = body:match('src="([^"]*ocover%.jpg[^"]*)"[^>]*class="detail-cover"')
    or body:match('class="detail-cover"[^>]*src="([^"]+)"')
    or body:match('src="([^"]*ocover%.jpg[^"]*)"')
    or body:match('property="og:image" content="([^"]+)"')
    or ""
  if cover:sub(1, 2) == "//" then cover = "https:" .. cover end

  -- genre chips; the cell markup changed once already (href=/directory/slug
  -- -> bare links), keep both shapes, deduped
  local cell = body:match([[Genre%(s%):</span>%s*.-</p>]]) or ""
  local tags, seen = {}, {}
  for g in cell:gmatch('href="/directory/[^"]*"[^>]*>([^<]+)</a>') do
    local t = host.text.trim(g)
    if t ~= "" and not seen[t] then
      seen[t] = true
      tags[#tags + 1] = t
    end
  end
  for name in cell:gmatch("%s*<a[^>]*>([^<]+)</a>") do
    local t = host.text.trim(name)
    if t ~= "" and not seen[t] then
      seen[t] = true
      tags[#tags + 1] = t
    end
  end

  local status = host.text.normalize_status(body:match(">(Ongoing)<") or body:match(">(Completed)<") or "")

  local desc = host.text.trim(host.text.unescape(
    body:match('<div[^>]*class="[^"]*content[^"]*"[^>]*>(.-)</div>')
      or body:match('name="description" content="([^"]*)"')
    or ""))

  return { ok = true, data = {
    id = id,
    name = title,
    cover = cover,
    status = status,
    tags = tags,
    release_at = 0, -- the page carries no machine-readable date
    description = desc,
    authors = {}, -- the page lists no author
    url = BASE .. "/manga/" .. id,
  } }
end
