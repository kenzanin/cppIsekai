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
  local body, err = get("/at-home/server/" .. (params.id or ""))
  if err ~= nil then return { ok = false, error = err } end
  local data = type(body.chapter) == "table" and body.chapter.data
  if type(body.baseUrl) ~= "string" or type(data) ~= "table" or #data == 0 then
    return { ok = false, error = "at-home: malformed response" }
  end
  local prefix = body.baseUrl .. "/data/" .. body.chapter.hash .. "/"
  local out = {}
  for _, f in ipairs(data) do out[#out + 1] = prefix .. f end
  return { ok = true, data = out }
end

