local c = require("common")
local BASE = c.BASE
local trim = c.trim
local http_req = c.http_req
local find_all = c.find_all
local epoch_ms_to_secs = c.epoch_ms_to_secs
local per_page = c.per_page
local page_cache = c.page_cache
local fetch_search = c.fetch_search
local page_info = c.page_info
return function(params)
  local id = tostring(params.id or "")
  if id == "" then
    return { ok = false, error = "comik: missing id" }
  end
  local body, err = http_req(BASE .. "/title/" .. id)
  if not body then
    return { ok = false, error = err or "comik: chapters failed" }
  end

  local srcs, sseen = {}, {}
  for sid in body:gmatch('href="/source/([a-z0-9]+)"') do
    if not sseen[sid] then
      sseen[sid] = true
      srcs[#srcs + 1] = sid
    end
  end

  -- Chapter row: <a href="/chapter/<id>" ...><label></a> ... <time data-time>.
  -- Plain Lua string ops: coregex lazy quantifiers proved unreliable here
  -- (source lesson, kept). Anchor on the link, read data-time within +400.
  local out, bynum = {}, {}
  local function harvest(pagebody)
    local pos = pagebody:find('href="/chapter/', 1, true)
    while pos do
      local cid = pagebody:match('/chapter/([a-z0-9]+)', pos + 15)
      local close = pagebody:find('</a>', pos, true)
      if not cid or not close then break end
      local window = pagebody:sub(pos, close + 400)
      local ms = window:match('data%-time="(%d+)"') or window:match('data-time="(%d+)"')
      local label = window:match('>([^<]+)</a>')
      if ms then
        local secs = epoch_ms_to_secs(ms)
        local prev = bynum[cid]
        if prev then
          -- keep the earliest release across groups
          if secs and (not prev.release_at or secs < prev.release_at) then
            prev.release_at = secs
          end
        else
          local num = label and label:match("(%d+%.?%d*)") or "0"
          local item = {
            id = cid,
            number = tonumber(num) or 0,
            title = host.text.unescape(trim(label or "")),
            release_at = secs or 0,
            pages = 0,
          }
          bynum[cid] = item
          out[#out + 1] = item
        end
      end
      pos = pagebody:find('href="/chapter/', close, true)
    end
  end

  harvest(body) -- latest on the title page first (single-source titles)
  for _, sid in ipairs(srcs) do
    local sb = http_req(BASE .. "/source/" .. sid)
    if sb then harvest(sb) end
  end

  if #out == 0 then
    return { ok = false, error = "comik: no chapters for " .. id }
  end
  -- newest first (sources render newest-first; the title page mixes)
  table.sort(out, function(a, b) return (a.number or 0) > (b.number or 0) end)
  return { ok = true, data = out }
end
