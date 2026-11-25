-- weebcentral.com — server-rendered (HTMX partials), no JSON API, no JS gate.
-- Endpoints consumed here (all plain HTML, offset-paginated search):
--   /search/data?text=q&limit=32&offset=N   HTMX result cards (one shot)
--   /search?included_tag=<Tag>              tag filtering (repeated params ok)
--   /series/<ULID>                          series page (301 -> /series/<ULID>/<Slug>)
--   /series/<ULID>/full-chapter-list        ALL chapter rows (HTMX partial)
--   /chapters/<ULID>/images?reading_style=long_strip&current_page=1   direct CDN <img>s
-- Stable id = ULID (series and chapter ULIDs both). Slugs are cosmetic.
local BASE = "https://weebcentral.com"
local UA = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36"
local LIMIT = 32

local STATUS = {
  complete = "completed",
  ongoing = "ongoing",
  hiatus = "hiatus",
  cancelled = "dropped",
  dropped = "dropped",
  upcoming = "upcoming",
}

-- Card = one top-level '<article class="bg-base-300' block; the desktop anchor
-- inside it carries ULID + slug + title, Year/Status/Tags live in the trailing
-- section of the same block. Scope = [previous sentinel, next sentinel).
-- site HTML carries entities like &#39; / &amp; / &#x27; — decode once so they
-- never reach the UI as literal text (the digits in &#39; read as "a number").

local function get(path)
  local res, err = host.http.fetch(
    BASE .. path,
    { headers = { ["User-Agent"] = UA, ["Accept"] = "text/html,*/*" }, timeout_ms = 30000 }
  )
  if err ~= nil then
    return nil, err
  end
  if res.status ~= 200 then
    return nil, "weebcentral: upstream status " .. tostring(res.status)
  end
  return res.body, nil
end

local function txt(s)
  return host.text.unescape(host.text.trim(s or ""))
end

local function cards(body)
  local out = {}
  if type(body) ~= "string" then
    return out
  end
  -- sentinel offsets for every top-level card
  local sentinels, s = {}, 1
  while true do
    local n = body:find('<article class="bg-base-300', s, true)
    if not n then
      break
    end
    sentinels[#sentinels + 1] = n
    s = n + 1
  end
  sentinels[#sentinels + 1] = #body + 1
  for i = 1, #sentinels - 1 do
    local scope = body:sub(sentinels[i], sentinels[i + 1] - 1)
    local ulid, slug, title = scope:match('([A-Z0-9]+)/([^"]*)" class="line%-clamp%-1 link link%-hover">([^<]+)</a>')
    if ulid and #ulid == 26 then
      local year = scope:match("<strong>Year:</strong>%s*<span>(%d+)</span>")
      local status = scope:match("<strong>Status:</strong>%s*<span>([^<]+)</span>")
      local authors = {}
      for name in scope:gmatch('search%?author=[^"]*"[^>]*>([^<]+)</a>') do
        authors[#authors + 1] = txt(name)
      end
      local tags = {}
      local tblock = scope:match("<strong>Tag%(s%):%s*</strong>(.-)</div>")
      if tblock then
        for t in tblock:gmatch("<span>([^<]+)</span>") do
          local tag = txt(t:gsub(",%s*$", ""))
          if tag ~= "" then
            tags[#tags + 1] = tag
          end
        end
      end
      out[#out + 1] = {
        id = ulid,
        name = txt(title),
        cover = "https://temp.compsci88.com/cover/fallback/" .. ulid .. ".jpg",
        status = STATUS[string.lower(status or "")] or "unknown",
        tags = tags,
        authors = authors,
        release_at = year and (year .. "-01-01") or 0,
        url = BASE .. "/series/" .. ulid .. "/" .. (slug or ""),
      }
    end
  end
  return out
end

return { BASE = BASE, UA = UA, LIMIT = LIMIT, STATUS = STATUS, get = get, txt = txt, cards = cards }
