local c = require("common")
local API = c.API
local SITE = c.SITE
local OPTS = c.OPTS
local get_json = c.get_json
local find1 = c.find1
local names = c.names
local epoch = c.epoch
local ensure_genres = c.ensure_genres
local slug_of = c.slug_of
return function(params)
  local composite = tostring(params.id or "")
  local id, chapter = composite:match("^([^:]+):(.+)$")
  if not id or not chapter then return { ok = true, data = {} } end
  local data, err = get_json("/titles/" .. host.url_encode(id) .. "/chapters/" .. host.url_encode(chapter))
  if not data then return { ok = false, error = err } end
  local c = data.chapter or {}
  local out = {}
  for _, url in ipairs(c.images or {}) do
    if type(url) == "string" and url ~= "" then out[#out + 1] = url end
  end
  return { ok = true, data = out }
end
