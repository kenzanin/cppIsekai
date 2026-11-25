local c = require("common")
local normalize_status = c.normalize_status
local API = c.API
local AUTH = c.AUTH
local have_creds = c.have_creds
local epoch = c.epoch
local token_post = c.token_post
local access_token = c.access_token
local get = c.get
local canonical_manga = c.canonical_manga
local tag_ids = c.tag_ids
return function()
  local map, err = tag_ids()
  if err ~= nil then return { ok = false, error = err } end
  local names = {}
  for name in pairs(map) do names[#names + 1] = name end
  table.sort(names)
  return { ok = true, data = names }
end

