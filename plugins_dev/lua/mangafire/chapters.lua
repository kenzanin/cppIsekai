local c = require("common")
local API_URL = c.API_URL
local REFERER = c.REFERER
local USER_AGENT = c.USER_AGENT
local VRF_K1 = c.VRF_K1
local VRF_T1 = c.VRF_T1
local VRF_K2 = c.VRF_K2
local VRF_T2 = c.VRF_T2
local VRF_K3 = c.VRF_K3
local VRF_T3 = c.VRF_T3
local VRF_STAGES = c.VRF_STAGES
local vrf_url = c.vrf_url
local http_get = c.http_get
local sanitize_title = c.sanitize_title
local strip_html = c.strip_html
local normalize_status = c.normalize_status
local GENRE_IDS = c.GENRE_IDS
local cover_of = c.cover_of
return function(params)
  local hid = params.id or ""
  if hid == "" then
    return { ok = true, data = {} }
  end
  local out, page = {}, 1
  -- up to 3 pages (200/page), newest-first upstream; sort ascending like mangadex
  -- (host routes pages/CBZ by chapter number and expects reading order)
  while page <= 3 do
    local url, verr = vrf_url("/titles/" .. hid .. "/chapters", {
      language = "en",
      limit = "200",
      order = "desc",
      page = tostring(page),
      sort = "number",
    })
    if verr ~= nil then
      return { ok = false, error = verr }
    end
    local body, err = http_get(url)
    if err ~= nil then
      return { ok = false, error = err }
    end
    local items = body.items or {}
    if #items == 0 then
      break
    end
    for _, c in ipairs(items) do
      if type(c) == "table" and c.id then
        -- upstream sends number=0 for entries that only carry a numeric name
        -- (e.g. title "48"); fall back to the leading number in the name.
        -- ponytail: a genuine prequel ch 0 whose name starts with a number
        -- (other than 0) would be renumbered; none seen on mangafire yet.
        local num = tonumber(c.number)
        if not num or num == 0 then
          num = tonumber(tostring(c.name or ""):match("^%s*(%d+%.?%d*)")) or num or 0
        end
        local cid = tonumber(c.id)
        out[#out + 1] = {
          id = cid and string.format("%.0f", cid) or tostring(c.id),
          number = num,
          title = c.name or "",
          release_at = tonumber(c.createdAt or c.created_at) or 0,
          pages = tonumber(c.pages) or tonumber(c.pageCount) or 0,
        }
      end
    end
    local meta = body.meta or {}
    local has_next = meta.hasNext or meta.has_next
    local last_page = tonumber(meta.lastPage or meta.last_page) or 1
    if not has_next or page >= last_page then
      break
    end
    page = page + 1
  end
  table.sort(out, function(a, b)
    return (a.number or 0) < (b.number or 0)
  end)
  return { ok = true, data = out }
end
