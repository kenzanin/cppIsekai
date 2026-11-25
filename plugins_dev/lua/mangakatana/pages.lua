local c = require("common")
local BASE = c.BASE
local opts = c.opts
local http_req = c.http_req
local find1 = c.find1
local slug_of = c.slug_of
local url_for = c.url_for
local chapter_url = c.chapter_url
local page_urls = c.page_urls
return function(params)
  local body, err = http_req(chapter_url(params.id or ""))
  if err ~= nil then return { ok = false, error = err } end

  local urls = page_urls(body)
  if #urls == 0 then return { ok = false, error = "mangakatana: no pages for " .. tostring(params.id) } end
  return { ok = true, data = urls }
end
