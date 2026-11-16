-- Page-list scraper. Returns function(ctx) -> handler(params).
-- Every page img is inlined as https://imgx.mghcdn.com/<slug>/<num>/<i>.<ext>;
-- the <i> tail gives the order (preload block and reader body both list them).
return function(ctx)
  return function(params)
    local composite = params.id or ""
    local slug, chid = string.match(composite, "^(.+):(chapter%-[%d%.]+)$")
    if not slug then
      return { ok = false, error = "1manga: chapter id must be slug:chapter-N, got " .. tostring(composite) }
    end

    local body, err = ctx.get(ctx.BASE .. "/chapter/" .. host.url_encode(slug) .. "/" .. chid)
    if not body then return { ok = false, error = err } end

    local chnum = string.match(chid, "chapter%-(.+)$")
    local pat = [[https://imgx\.mghcdn\.com/]] .. slug .. "/" .. chnum .. [[/([0-9]+)\.[a-z]+]]
    local by_index, order = {}, {}
    for _, m in ipairs(host.re2.find_all(pat, body)) do
      local idx = tonumber(m[2])
      if idx and not by_index[idx] then
        by_index[idx] = m[1]
        order[#order + 1] = idx
      end
    end
    table.sort(order)
    local out = {}
    for _, idx in ipairs(order) do out[#out + 1] = by_index[idx] end
    if #out == 0 then
      host.log.error("1manga: no page images in " .. composite)
      return { ok = false, error = "1manga: no pages for " .. composite }
    end
    host.log.info("1manga: found " .. tostring(#out) .. " pages for " .. composite)
    return { ok = true, data = out }
  end
end
