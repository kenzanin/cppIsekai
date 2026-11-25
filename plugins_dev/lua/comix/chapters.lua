local c = require("common")
local BASE = c.BASE
local render, find1, cover_abs = c.render, c.find1, c.cover_abs
local initial_data, relative_to_iso = c.initial_data, c.relative_to_iso
-- Chapter rows are client-rendered AND client-paginated: the title page
-- carries 20 rows plus a .npager ("Showing 1 to 20 of N", num buttons) —
-- no per-page URL, the pager is buttons. The harvest is host-driven now:
-- click the next .npager__num, let CDP wait for the hint to name the page
-- (browser.wait polls !!expr server-side -- no hand-rolled timers), absorb
-- rows. State lives in window.__chx; dedupe by href; cap 40 pages.
local CH_RESET = 'window.__chx={all:{},order:[]};"1"'

local CH_ABSORB =
  [[(function(){var s=window.__chx||(window.__chx={all:{},order:[]});Array.prototype.slice.call(document.querySelectorAll("a.mchap-row__primary")).forEach(function(a){var r=a.closest(".mchap-row")||a.parentElement;var c=r&&r.querySelector(".mchap-row__ch");var t=r&&r.querySelector(".mchap-row__time");var h=a.getAttribute("href")||"";if(h&&!(h in s.all)){s.all[h]=1;s.order.push({h:h,c:c?c.textContent.trim():"",t:t?t.textContent.trim():""});}});var q=document.querySelector(".mchap-foot__hint");return JSON.stringify({n:s.order.length,q:q?q.textContent.replace(/\\s+/g," "):""});})()]]

local function ch_eval(js, tmo)
  local out, err = host.browser.call("evaluate", { script = js, timeout_ms = tmo or 15000 })
  if err ~= nil or out == nil or out == "" then
    return nil
  end
  return out
end

local function ch_absorb()
  local out = ch_eval(CH_ABSORB)
  if not out then
    return nil
  end
  local ok, t = pcall(json.decode, out)
  if not ok or type(t) ~= "table" then
    return nil
  end
  return t
end

local function ch_click(n)
  local out = ch_eval(
    '(function(){var bs=document.querySelectorAll(".npager__num");for(var i=0;i<bs.length;i++){if(bs[i].textContent.trim()===String('
      .. n
      .. ')){bs[i].click();return "1";}}return "";})()'
  )
  return out == "1"
end

local function ch_wait_count(want, tmo)
  -- poll with a side effect: every tick absorbs visible rows, then requires
  -- the running count -- the wait IS the collector, no settle/race afterwards
  local _, err = host.browser.call("wait", {
    timeout_ms = tmo or 8000,
    expression = '(function(){var s=window.__chx||(window.__chx={all:{},order:[]});Array.prototype.slice.call(document.querySelectorAll("a.mchap-row__primary")).forEach(function(a){var r=a.closest(".mchap-row")||a.parentElement;var c=r&&r.querySelector(".mchap-row__ch");var t=r&&r.querySelector(".mchap-row__time");var h=a.getAttribute("href")||"";if(h&&!(h in s.all)){s.all[h]=1;s.order.push({h:h,c:c?c.textContent.trim():"",t:t?t.textContent.trim():""});}});return s.order.length>='
      .. want
      .. ";})()",
  })
  return err == nil
end

local function ch_final()
  local out = ch_eval("JSON.stringify(window.__chx&&window.__chx.order||[])")
  if not out then
    return nil
  end
  local ok, rows = pcall(json.decode, out)
  if ok and type(rows) == "table" then
    return rows
  end
  return nil
end

-- One pass; nil on any host/parse failure (caller retries once via fresh goto).
local function ch_harvest()
  local _, eerr = host.browser.call(
    "wait",
    { expression = 'document.querySelector("a.mchap-row__primary")!==null', timeout_ms = 15000 }
  )
  if eerr ~= nil then
    return nil
  end
  if ch_eval(CH_RESET) == nil then
    return nil
  end
  local t = ch_absorb()
  if not t then
    return nil
  end
  local total = tonumber(t.q and t.q:match("of%s+(%d+)")) or 0
  local pp = tonumber(t.q and t.q:match("to%s+(%d+)%s+of")) or 0
  if pp > 0 then
    for n = 2, 40 do
      if total > 0 and t.n >= total then
        break
      end
      if not ch_click(n) then
        break
      end
      local want = math.min(total > 0 and total or n * pp, n * pp)
      if not ch_wait_count(want) then
        ch_click(n) -- one belt click, re-wait, absorb whatever rendered
        ch_wait_count(want, 4000)
      end
      local t2 = ch_absorb()
      if not t2 then
        return nil
      end
      t = t2
    end
  end
  return ch_final()
end

return function(params)
  local slug = params.id or ""
  if slug == "" then
    return { ok = true, data = {} }
  end

  local _, gerr = host.browser.call("goto", { url = BASE .. "/title/" .. host.url_encode(slug), timeout_ms = 60000 })
  if gerr ~= nil then
    return { ok = false, error = "comix: goto failed: " .. gerr }
  end
  local rows = ch_harvest()
  if rows == nil then
    return { ok = false, error = "comix: harvest failed for " .. slug }
  end
  if #rows == 0 then
    -- upstream /api/v1 sits behind an on/off CF turnstile; one clean retry
    -- (fresh goto re-runs the page's own challenge path) before giving up
    local _, _ = host.browser.call("goto", { url = BASE .. "/title/" .. host.url_encode(slug) })
    rows = ch_harvest()
    if rows == nil then
      return { ok = false, error = "comix: harvest failed for " .. slug }
    end
  end

  local out = {}
  for _, r in ipairs(rows) do
    local href, chtxt, when = r.h or "", r.c or "", r.t or ""
    local chid = string.match(href or "", "/([^/]+)$") or ""
    if chid ~= "" then
      -- Chapter number from "Ch.43.2" or the URL tail.
      local num = string.match(chtxt or "", "Ch%.([%d%.]+)") or string.match(chid, "chapter%-([%d%.]+)")
      local item = {
        id = slug .. ":" .. chid,
        number = tonumber(num) or 0,
        title = chtxt or "",
        release_at = 0,
        pages = 0,
      }
      local iso = relative_to_iso(when)
      if iso ~= "" then
        item.release_at = iso
      end
      out[#out + 1] = item
    end
  end
  if #out == 0 then
    host.log.error("comix: no chapter rows rendered for " .. slug)
    return { ok = false, error = "comix: no chapters for " .. slug }
  end
  host.log.info("comix: found " .. tostring(#out) .. " chapters for " .. slug)
  -- Site renders newest-first; keep document order.
  return { ok = true, data = out }
end
