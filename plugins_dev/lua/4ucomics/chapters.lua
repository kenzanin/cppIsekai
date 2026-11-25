local c = require("common")
local API = c.API
local STATUS = c.STATUS
local proxy_img = c.proxy_img
local status_of = c.status_of
local get_json = c.get_json
local number_of = c.number_of
return function(params)
  local slug = params.id or ""
  if slug == "" then return { ok = true, data = {} } end
  local d, err = get_json("/manga/" .. slug)
  if err then return { ok = false, error = err } end
  local m = d.manga or {}
  local out = {}
  for _, c in ipairs(m.chapters or {}) do
    local name = tostring(c.name or c.rawChapter or "")
    if name ~= "" then
      local dstr = tostring(c.date or "")
      out[#out + 1] = {
        id = slug .. ":" .. name,
        number = number_of(name),
        title = "",
        release_at = (dstr ~= "") and dstr:sub(1, 10) or 0,
        created_at = (dstr ~= "") and dstr:sub(1, 10) or 0,
        pages = 0,
      }
    end
  end
  return { ok = true, data = out }
end
