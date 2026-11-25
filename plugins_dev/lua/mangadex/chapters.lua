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
  -- MangaDex wants array-style params: translatedLanguage[]=en, order[chapter]=asc.
  local body, err = get(
    "/manga/" .. (params.id or "") .. "/feed",
    { ["translatedLanguage[]"] = params.lang or "en", limit = params.limit or 100, ["order[chapter]"] = "asc" }
  )
  if err ~= nil then
    return { ok = false, error = err }
  end
  local out = {}
  for _, c in ipairs(body.data or {}) do
    local attrs = c.attributes or {}
    out[#out + 1] = {
      id = c.id,
      number = tonumber(attrs.chapter) or 0,
      title = attrs.title or "",
      release_at = epoch(attrs.publishAt),
      pages = tonumber(attrs.pages) or 0,
    }
  end
  return { ok = true, data = out }
end

-- Page images for a chapter: /at-home/server/{id} -> {baseUrl, chapter{hash,data[]}}.
-- data[] is the quality pass; dataSaver[] deliberately unused (dataSaver off).
-- baseUrl already carries the scheme (live: "https://<host>.mangadex.network").
