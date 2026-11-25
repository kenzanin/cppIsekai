-- Pages: GET /series/comic/<slug>/chapter/<n>. Reader rows are
-- <li class="ks-page"><img src="/media/pages/..." width height> in DOM order;
-- covers/thumbs live under /media/covers/ so the path prefix is exact.
-- Extension-agnostic by construction (path prefix, no ext pin).
local IMG = [[src="(/media/pages/[^"]+)"]]

return function(ctx)
  return function(params)
    local id = tostring(params.id or "")
    local slug, num = id:match("^(.+):([0-9.]+)$") -- subchapters: slug:25.2
    if slug == nil then
      return { ok = false, error = "keroscans: bad chapter id " .. id }
    end

    local body, err = ctx.get(ctx.BASE .. "/series/comic/" .. slug .. "/chapter/" .. num)
    if not body then
      return { ok = false, error = err }
    end

    local out = {}
    for _, m in ipairs(host.re2.find_all(IMG, body)) do
      out[#out + 1] = ctx.BASE .. m[2]
    end
    if #out == 0 then
      if body:find("ks-reader-locked", 1, true) then
        return { ok = false, error = "keroscans: chapter locked (paid) " .. id }
      end
      return { ok = false, error = "keroscans: no pages for " .. id }
    end
    host.log.info("keroscans: " .. tostring(#out) .. " pages for " .. id)
    return { ok = true, data = out }
  end
end
