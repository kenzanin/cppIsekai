local c = require("common")
local API = c.API
local STATUS = c.STATUS
local proxy_img = c.proxy_img
local status_of = c.status_of
local get_json = c.get_json
local number_of = c.number_of
return function(params)
  local cid = params.id or ""
  local slug, sep, name = cid:match("^(.-)(:)(.+)$")
  if not sep then return { ok = false, error = "4ucomics: malformed chapter id " .. cid } end
  local d, err = get_json("/manga/" .. slug .. "?chapter=" .. host.url_encode(name))
  if err then return { ok = false, error = err } end
  local m = d.manga or {}
  for _, c in ipairs(m.chapters or {}) do
    if tostring(c.name or c.rawChapter or "") == name then
      local out = {}
      for _, p in ipairs(c.pages or {}) do
        local urls = p.pageURLs or {}
        if urls[1] then out[#out + 1] = proxy_img(urls[1]) end
      end
      return { ok = true, data = out }
    end
  end
  return { ok = false, error = "4ucomics: chapter not found " .. name }
end
