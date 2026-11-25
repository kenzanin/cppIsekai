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
  -- MangaDex related-reading: /manga/<id>/recommendation. Each entry carries a
  -- `score` (attrs.score) and relationships[] pointing at the candidate manga;
  -- the OTHER half of each pair (not the asked id) is the candidate.
  local body, err = get("/manga/" .. (params.id or "") .. "/recommendation")
  if err ~= nil then return { ok = false, error = err } end
  local id = params.id or ""
  local scored = {}
  for _, entry in ipairs(body.data or {}) do
    local score = tonumber((entry.attributes or {}).score) or 0
    for _, rel in ipairs(entry.relationships or {}) do
      if rel.type == "manga" and rel.id and rel.id ~= id then
        scored[#scored + 1] = { id = rel.id, score = score }
        break
      end
    end
  end
  table.sort(scored, function(a, b) return a.score > b.score end)
  if #scored == 0 then return { ok = true, data = { items = {} } } end
  local ids = {}
  for _, s in ipairs(scored) do ids[#ids + 1] = s.id end
  local bbody, berr = get("/manga", { ["ids[]"] = ids, ["includes[]"] = "cover_art", limit = #ids })
  if berr ~= nil then return { ok = false, error = berr } end
  -- preserve score order from the recommendation call
  local by_id = {}
  for _, m in ipairs(bbody.data or {}) do by_id[m.id] = canonical_manga(m) end
  local items = {}
  for _, s in ipairs(scored) do
    if by_id[s.id] then items[#items + 1] = by_id[s.id] end
  end
  return { ok = true, data = { items = items } }
end

