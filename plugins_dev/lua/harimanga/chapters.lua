local c = require("common")
local BASE = c.BASE
local find1 = c.find1
return function(params)
  local slug = params.id or ""
  if slug == "" then
    return { ok = true, data = {} }
  end
  -- Madara chapters via POST /manga/<slug>/ajax/chapters/. CF eats curl POSTs
  -- too: goto the title page (challenge solves itself), then same-origin XHR
  -- with the session cookies, no jar minting (comix pattern).
  local _, gerr = host.browser.call("goto", { url = BASE .. "/manga/" .. slug .. "/", timeout_ms = 60000 })
  if gerr ~= nil then
    return { ok = false, error = "harimanga: goto failed: " .. gerr }
  end
  local _, werr = host.browser.call(
    "wait",
    { expression = "document.title.indexOf('Just a moment')===-1", timeout_ms = 20000 }
  )
  if werr ~= nil then
    return { ok = false, error = "harimanga: challenge stuck on manga/" .. slug }
  end
  local out, eerr = host.browser.call(
    "evaluate",
    {
      script = [[(function(){var x=new XMLHttpRequest();x.open("POST","/manga/]]
        .. slug
        .. [[/ajax/chapters/",false);x.setRequestHeader("X-Requested-With","xmlhttprequest");x.setRequestHeader("Content-Type","application/x-www-form-urlencoded");x.send("");return JSON.stringify({s:x.status,b:x.responseText});})()]],
      timeout_ms = 30000,
    }
  )
  if eerr ~= nil or out == nil or out == "" then
    return { ok = false, error = "harimanga: ajax chapters failed: " .. tostring(eerr) }
  end
  local ok, env = pcall(json.decode, out or "")
  local status = ok and type(env) == "table" and tonumber(env.s) or nil
  local body = ok and type(env) == "table" and env.b or nil
  if status ~= 200 or not body or body == "" then
    return { ok = false, error = "harimanga: ajax chapters status " .. tostring(status) }
  end

  local rows, seen = {}, {}
  for _, m in ipairs(host.re2.find_all([[(?s)<li[^>]*class="wp-manga-chapter[^"]*"[^>]*>(.*?)</li>]], body)) do
    local li = m[1]
    local href = find1(li, [[href="([^"]+)"]])
    local label = host.text.trim(find1(li, [[>([^<]*[Cc]hapter[^<]*)<]]) or "")
    if href and label ~= "" then
      local cid = find1(href, [[/manga/[^/]+/([^/]+)/?$]]) or find1(href, [[/([^/]+)/?$]])
      if cid and not seen[cid] then
        seen[cid] = true
        rows[#rows + 1] = {
          id = slug .. ":" .. cid, -- composite slug:chapterId; pages needs both
          number = host.text.chapter_num(label) or 0,
          title = label,
          release_at = 0, -- Madara dates are human strings; host wants epoch ints (anisascans precedent)
          pages = 0,
        }
      end
    end
  end

  -- fallback: chapter links in the page body (theme variants without the li)
  if #rows == 0 then
    for _, m in ipairs(host.re2.find_all([[<a[^>]*href="([^"]*chapter-[^"]+)"[^>]*>\s*([^<]+)\s*</a>]], body)) do
      local href, label = m[2], m[3]
      local cid = href and find1(href, [[/([^/]+)/?$]])
      if cid and not seen[cid] then
        seen[cid] = true
        rows[#rows + 1] = {
          id = slug .. ":" .. cid,
          number = host.text.chapter_num(label) or 0,
          title = host.text.trim(label or ""),
          release_at = 0,
          pages = 0,
        }
      end
    end
  end

  table.sort(rows, function(a, b)
    return (a.number or 0) < (b.number or 0)
  end)
  return { ok = true, data = rows }
end
