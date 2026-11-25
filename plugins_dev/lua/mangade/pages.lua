local c = require("common")
local API = c.API
local SITE = c.SITE
local OPTS = c.OPTS
local get_json = c.get_json
local find1 = c.find1
local clean = c.clean
local epoch = c.epoch
local split_names = c.split_names
local names = c.names
local comic_id = c.comic_id
local genre_ids = c.genre_ids
return function(params)
  local composite = tostring(params.id or "")
  local _, chapter = composite:match("^([^:]+):(.+)$")
  if not chapter then
    return { ok = true, data = {} }
  end
  local d, err = get_json("/chapters/" .. host.url_encode(chapter) .. "/view")
  if not d then
    return { ok = false, error = err }
  end
  local out = {}
  for _, img in ipairs(d.chapter_images or {}) do
    local url = type(img) == "table" and img.image or img
    if type(url) == "string" and url ~= "" then
      out[#out + 1] = url
    end
  end
  return { ok = true, data = out }
end
