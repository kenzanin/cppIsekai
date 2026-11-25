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
return function(params)
  local body, err = get("/manga/" .. (params.id or ""), { ["includes[]"] = { "cover_art", "author", "artist" } })
  if err ~= nil then return { ok = false, error = err } end
  return { ok = true, data = canonical_manga(body.data) }
end

