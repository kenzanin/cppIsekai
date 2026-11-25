local c = require("common")
local BASE = c.BASE
local trim = c.trim
local get_json = c.get_json
local absolute = c.absolute
return function(params)
  local slug = tostring(params.id or "")
  local data = get_json(BASE .. "/series/" .. host.url_encode(slug) .. ".json")

  local chapters = {}
  for _, tag in ipairs((data and data.taggings) or {}) do
    local permalink = tag.permalink
    if permalink and permalink ~= "" then
      local label = trim(tag.title or "")
      chapters[#chapters + 1] = {
        id = permalink,
        number = host.text.chapter_num(label) or 0,
        title = label,
        -- taggings carries no dates
        release_at = 0,
        pages = 0,
      }
    end
  end

  -- taggings is newest-first already; the explicit re-sort is cheap insurance
  table.sort(chapters, function(a, b)
    return (a.number or 0) > (b.number or 0)
  end)
  return { ok = true, data = chapters }
end
