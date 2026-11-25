local c = require("common")
local BASE = c.BASE
local OPTS = c.OPTS
local http_req = c.http_req
local find1 = c.find1
local clean = c.clean
local first = c.first
local get_json = c.get_json
local json_url = c.json_url
local split_id = c.split_id
local tags_of = c.tags_of
return function(params)
  local dir, permalink = split_id(params.id)
  if not dir or not permalink then return { ok = true, data = {} } end
  local doc, err = get_json(json_url(dir, permalink))
  if not doc then return { ok = false, error = err } end
  if dir == "chapters" then
    return { ok = true, data = { { id = dir .. ":" .. permalink, number = 0,
                                   title = clean(doc.title or "Chapter"), release_at = host.text.date_epoch(tostring(doc.released_on or "")) or 0,
                                   pages = #(doc.pages or {}) } } }
  end
  local out, header = {}, ""
  for _, item in ipairs(doc.taggings or {}) do
    if item.header then
      header = clean(item.header)
    elseif item.permalink then
      local title = clean(item.title or "Chapter")
      if header ~= "" then title = header .. " " .. title end
      out[#out + 1] = {
        id = "chapters:" .. item.permalink,
        number = host.text.chapter_num(title) or 0,
        title = title,
        release_at = host.text.date_epoch(tostring(item.released_on or "")) or 0,
        pages = 0,
      }
    end
  end
  table.sort(out, function(a, b) return a.number < b.number end)
  return { ok = true, data = out }
end
