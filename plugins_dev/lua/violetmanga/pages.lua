local c = require("common")
local BASE = c.BASE
local http_req = c.http_req
local sel1 = c.sel1
return function(params)
  local id = params.id or ""
  local url = tostring(id)
  if not url:match("^https?://") then
    url = BASE .. id
  end
  local body, err = http_req(url)
  if err ~= nil then
    return { ok = false, error = err }
  end

  -- ts_reader.run({..., "sources": [{..., "images": [...]}]}): scan inline
  -- scripts for the "images" array; slashes may be JSON-escaped (\/).
  local images = body:match('"images"%s*:%s*%[(.-)%]')
  if not images then
    host.log.error("violetmanga: no images array in " .. url)
    return { ok = false, error = "violetmanga: no pages for " .. tostring(id) }
  end
  local out = {}
  for raw in images:gmatch("https?:\\?/\\?/[^\"']+") do
    out[#out + 1] = (raw:gsub("\\/", "/"))
  end
  if #out == 0 then
    host.log.error("violetmanga: no page images in " .. url)
    return { ok = false, error = "violetmanga: no pages for " .. tostring(id) }
  end
  return { ok = true, data = out }
end
