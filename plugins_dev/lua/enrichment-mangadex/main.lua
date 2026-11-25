local c = require("common")
local API = c.API
local normalize_status = c.normalize_status
local CDN = c.CDN
local SITE = c.SITE
local PREFERRED = c.PREFERRED
local pick = c.pick
local get = c.get
local clean_text = c.clean_text
local normalize = c.normalize
local best_record = c.best_record
local extract_tags = c.extract_tags
local extract_names = c.extract_names
local cover_urls = c.cover_urls
local build_recommendations = c.build_recommendations
local alt_titles = c.alt_titles
local build_detail = c.build_detail
local build_covers = c.build_covers

function handle(req)
  local p = req.params or {}
  local title = tostring(p.title or "")
  local kind = tostring(p.kind or "detail")
  if title == "" then
    return { ok = false, error = "missing title" }
  end
  if kind ~= "detail" and kind ~= "covers" and kind ~= "recommendations" then
    return { ok = false, error = "kind must be detail|covers|recommendations" }
  end
  if #title > 512 then
    return { ok = false, error = "title too long" }
  end

  local rec = best_record(title)
  if not rec then
    return { ok = false, error = "no match found" }
  end

  if kind == "recommendations" then
    return build_recommendations(rec)
  end
  local data = kind == "covers" and build_covers(rec) or build_detail(rec)
  return { ok = true, data = data }
end
