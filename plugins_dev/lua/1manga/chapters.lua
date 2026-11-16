-- Chapter-list scraper. Returns function(ctx) -> handler(params).
-- One <li class="list-group-item"> per chapter. Each row carries TWO anchors:
-- the real chapter link first, then a mirror anchor whose numeric tail is a
-- junk id (chapter-2321739 rendered for chapter 3859) -- so read the first
-- /chapter/ href in the row and take the display number from "#N".
local ROW = [[(?s)class="[^"]*list-group-item[^"]*"(.*?)</li>]]
local NUM = [[>#([0-9.]+)<]] -- after decomment, "#<!-- -->3862" collapses to "#3862"

return function(ctx)
  local t = ctx.text

  return function(params)
    local slug = params.id or ""
    if slug == "" then return { ok = true, data = {} } end

    local body, err = ctx.get(ctx.BASE .. "/manga/" .. host.url_encode(slug))
    if not body then return { ok = false, error = err } end

    -- RE2 here, not Lua patterns: the slug carries '-' (a Lua lazy quantifier)
    -- and the display number sits behind an HTML comment ("#<!-- -->3862").
    local want = [[href="https://1manga\.co/chapter/]] .. slug .. [[/(chapter-[0-9.]+)"]]
    local out, seen = {}, {}
    for _, m in ipairs(host.re2.find_all(ROW, body)) do
      local li = t.decomment(m[2])
      local nhit = host.re2.find(NUM, li)
      local shown = tonumber(nhit and nhit[2])
      -- A row can list the mirror anchor first (chapter-367389 for chapter 2), so
      -- pick the href whose numeric tail matches the displayed #N; else the first.
      local chid, first
      for _, h in ipairs(host.re2.find_all(want, li)) do
        first = first or h[2]
        if shown and tonumber(string.match(h[2], "chapter%-(.+)")) == shown then
          chid = h[2]
          break
        end
      end
      chid = chid or first
      if chid and not seen[chid] then
        seen[chid] = true
        local mdd = t.iso_date(string.match(li, "<small[^>]*>([^<]*)</small>"))
        -- Custom chapter label ("- One Year Promise"); the default label
        -- ("- Chapter 3862") is dropped so the UI renders just the number.
        local label = t.clean_title(t.unesc(string.match(li, '<span class="_2IG5P">%s*%-%s*([^<]*)</span>') or ""))
        out[#out + 1] = {
          id = slug .. ":" .. chid,
          number = shown or tonumber(string.match(chid, "chapter%-(.+)")) or 0,
          title = label,
          release_at = (mdd ~= "") and mdd or 0,
          created_at = (mdd ~= "") and mdd or 0,
          pages = 0,
        }
      end
    end
    if #out == 0 then
      host.log.error("1manga: no chapter rows for " .. slug)
      return { ok = false, error = "1manga: no chapters for " .. slug }
    end
    host.log.info("1manga: found " .. tostring(#out) .. " chapters for " .. slug)
    return { ok = true, data = out } -- newest-first; the host sorts by number
  end
end
