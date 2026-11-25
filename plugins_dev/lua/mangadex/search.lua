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
  local q = params.title or params.query or params.q or ""
  local page = math.max(1, tonumber(params.page) or 1)
  local limit = tonumber(params.limit) or 12
  -- followCount desc = the site's ranking (one piece -> One Piece first);
  -- bare title order is server-default and buried the obvious match.
  local query = {
    title = q,
    limit = limit,
    ["order[followedCount]"] = "desc",
    offset = (page - 1) * limit,
    ["includes[]"] = "cover_art",
  }
  if params.tags and #params.tags > 0 then -- "Isekai,Fantasy" -> includedTags[] uuid, AND
    local map, err = tag_ids()
    if err ~= nil then
      return { ok = false, error = err }
    end
    local uuids = {}
    for name in params.tags:gmatch("[^,]+") do
      local id = map[name:gsub("^%s+", ""):gsub("%s+$", "")]
      if id then
        uuids[#uuids + 1] = id
      end
    end
    if #uuids > 0 then
      query["includedTags[]"] = uuids
    end
  end
  local body, err = get("/manga", query)
  if err ~= nil then
    return { ok = false, error = err }
  end
  local items = {}
  for _, m in ipairs(body.data or {}) do
    items[#items + 1] = canonical_manga(m)
  end
  return { ok = true, data = { items = items, total = tonumber(body.total) or #items } }
end
