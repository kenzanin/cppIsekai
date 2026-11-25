-- Chapters. Page 1 is server-rendered inside the series page; older pages are
-- GET /series/comic/<slug>?page=N&partial=chapters (full #chapter-list wrapper,
-- 10 rows per page). Loop until a fetch adds no rows.
local ROW = [[(?s)<li[^>]*data-access[^>]*>(.*?)</li>]]
local NUM = [[data-chapter-number="([0-9.]+)"]] -- subchapters: 25.2
local DATE = [[datetime="([^"]+)"]]

return function(ctx)
  return function(params)
    local slug = tostring(params.id or "")
    if slug == "" then
      return { ok = true, data = {} }
    end

    local out, seen = {}, {}

    local function harvest(body)
      local added = 0
      for _, m in ipairs(host.re2.find_all(ROW, body)) do
        local li = m[2]
        local nm = host.re2.find(NUM, li)
        local num = nm and tonumber(nm[2])
        local key = nm and nm[2]
        if num and key and not seen[key] then
          seen[key] = true
          local at = 0
          local d = host.re2.find(DATE, li)
          if d then
            local e = host.text.date_epoch(d[2])
            if type(e) == "number" then
              at = e
            end
          end
          out[#out + 1] = {
            id = slug .. ":" .. key,
            number = num,
            title = "",
            release_at = at,
            created_at = at,
            pages = 0,
          }
          added = added + 1
        end
      end
      return added
    end

    local body, err = ctx.get(ctx.BASE .. "/series/comic/" .. slug)
    if not body then
      return { ok = false, error = err }
    end
    harvest(body)

    for page = 2, 200 do
      local part
      part, err = ctx.get(ctx.BASE .. "/series/comic/" .. slug .. "?page=" .. page .. "&partial=chapters")
      if not part then
        if not (err and err:find("404", 1, true)) then
          host.log.error("keroscans: chapters truncated at page " .. page .. " for " .. slug .. ": " .. tostring(err))
        end
        break
      end
      if harvest(part) == 0 then
        break
      end
    end

    if #out == 0 then
      host.log.error("keroscans: no chapter rows for " .. slug)
      return { ok = false, error = "keroscans: no chapters for " .. slug }
    end
    host.log.info("keroscans: " .. tostring(#out) .. " chapters for " .. slug)
    return { ok = true, data = out } -- mixed order; the host sorts by number
  end
end
