local c = require("common")
local API = c.API
local STATUS = c.STATUS
local proxy_img = c.proxy_img
local status_of = c.status_of
local get_json = c.get_json
local number_of = c.number_of
return function(params)
  local slug = params.id or ""
  if slug == "" then return { ok = false, error = "4ucomics: missing id" } end
  local d, err = get_json("/manga/" .. slug)
  if err then return { ok = false, error = err } end
  local m = d.manga or {}
  local authors = {}
  local function names_of(raw)
    if type(raw) == "string" then return { raw } end
    if type(raw) ~= "table" then return {} end
    local out = {}
    if raw[1] ~= nil then
      for _, a in ipairs(raw) do out[#out + 1] = tostring(a) end
      return out
    end
    for _, a in pairs(raw) do out[#out + 1] = tostring(a) end
    return out
  end
  for _, a in ipairs(names_of(m.authors)) do authors[#authors + 1] = a end
  -- the API carries no series start date; the earliest chapter date is the only
  -- real one available (and the sort order makes "first" = oldest here anyway).
  local first = ""
  for _, c in ipairs(m.chapters or {}) do
    local ds = tostring(c.date or ""):sub(1, 10)
    if ds ~= "" and (first == "" or ds < first) then first = ds end
  end
  return { ok = true, data = {
    id = m.slug or slug,
    name = m.title or "",
    cover = proxy_img(m.cover or ""),
    status = status_of(m.statuses),
    tags = m.genres or {},
    authors = authors,
    artists = {},
    description = m.description or "",
    release_at = (first ~= "") and first or 0,
    url = "https://4ucomics.com/manga/" .. (m.slug or slug),
  } }
end
