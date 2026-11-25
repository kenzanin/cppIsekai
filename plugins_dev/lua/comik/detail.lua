local c = require("common")
local BASE = c.BASE
local trim = c.trim
local http_req = c.http_req
local find_all = c.find_all
local epoch_ms_to_secs = c.epoch_ms_to_secs
local per_page = c.per_page
local page_cache = c.page_cache
local fetch_search = c.fetch_search
local page_info = c.page_info
return function(params)
  local id = tostring(params.id or "")
  if id == "" then
    return { ok = false, error = "comik: missing id" }
  end
  local body, err = http_req(BASE .. "/title/" .. id)
  if not body then
    return { ok = false, error = err or "comik: detail failed" }
  end

  local name =
    host.text.unescape(trim((body:match("<title[^>]*>([^<]*)</title>") or id):gsub("%s*::%s*COMIK%.TO%s*$", "")))

  -- first large _f webp in the page header area
  local cover = body:match('<img[^>]*src="(/_f/[^"]+%.webp)"') or ""

  -- genre chips are bare words; keep the known-genre set (source parity)
  local genres, gseen = {}, {}
  for g in body:gmatch(">(%u%a+)<") do
    local gl = g:lower()
    if
      not gseen[gl]
      and (
        gl == "action"
        or gl == "adventure"
        or gl == "comedy"
        or gl == "drama"
        or gl == "fantasy"
        or gl == "romance"
        or gl == "isekai"
        or gl == "horror"
        or gl == "mystery"
        or gl == "sci"
        or gl == "slice"
        or gl == "sports"
        or gl == "supernatural"
        or gl == "thriller"
      )
    then
      gseen[gl] = true
      genres[#genres + 1] = gl
    end
  end

  local status = body:match(">(Ongoing)<") and "ongoing" or (body:match(">(Completed)<") and "completed" or "")

  return {
    ok = true,
    data = {
      id = id,
      name = name,
      cover = cover:find("^http") and cover or (cover ~= "" and BASE .. cover or ""),
      status = host.text.normalize_status(status),
      tags = genres,
      -- no machine date on the title page
      release_at = 0,
      description = "",
      authors = {},
      artists = {},
      url = BASE .. "/title/" .. id,
    },
  }
end
