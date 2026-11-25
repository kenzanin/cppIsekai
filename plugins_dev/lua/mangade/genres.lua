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
return function()
  local data, err = get_json("/genres?size=500")
  if not data then
    return { ok = false, error = err }
  end
  return { ok = true, data = names(data.genres) }
end
