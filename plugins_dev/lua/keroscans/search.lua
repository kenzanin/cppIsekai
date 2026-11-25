-- Search: GET /search?q=<q>. One <article> per result; slug link, first cover
-- img, title in <h3>. No pagination markers observed in the capture.
local CARD = [[(?s)<article[^>]*data-series-id[^>]*>(.*?)</article>]]

local function clean(s)
  return host.text.trim((host.text.unescape(s or ""):gsub("%s+", " ")))
end

return function(ctx)
  return function(params)
    local q = host.text.trim(tostring(params.title or params.query or params.q or ""))
    if q == "" then
      return { ok = true, data = { items = {}, total = 0 } }
    end
    local body, err = ctx.get(ctx.BASE .. "/search?q=" .. host.url_encode(q))
    if not body then
      return { ok = false, error = err }
    end

    local out, seen = {}, {}
    for _, m in ipairs(host.re2.find_all(CARD, body)) do
      local block = m[2]
      local slug = block:match('href="/series/comic/([^"]+)"')
      local cover = block:match('img src="([^"]+)"') or ""
      local name = clean(block:match("<h3[^>]*>(.-)</h3>") or "")
      if slug ~= nil and name ~= "" and not seen[slug] then
        seen[slug] = true
        if cover ~= "" and cover:sub(1, 1) == "/" then
          cover = ctx.BASE .. cover
        end
        out[#out + 1] = { id = slug, name = name, cover = cover }
      end
    end
    if #out == 0 then
      host.log.info("keroscans: search 0 for " .. q)
    end
    return { ok = true, data = { items = out, total = #out } }
  end
end
