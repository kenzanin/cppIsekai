-- helpers.lua — MangaBuddy transport/status helpers.
-- Port of goIsekai examples/plugins/lua/mangabuddy/helpers.lua as a require()d
-- module (cppIsekai has no pre-executed globals). http_get returns the body
-- or nil on transport failure / non-200.
local M = {}

local BASE = "https://mangabuddy1.co.uk" -- mirrors main.lua; default Referer origin

local DEFAULT_UA =
  "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36"

-- body or nil. opts.json switches Accept to application/json; opts.referer
-- overrides the BASE-root Referer, false sends none; opts.headers merge last.
function M.http_get(url, opts)
  local o = type(opts) == "table" and opts or (opts == true and { json = true } or {})
  local headers = {
    ["User-Agent"] = DEFAULT_UA,
    ["Accept"] = "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
    ["Accept-Language"] = "en-US,en;q=0.9",
  }
  if o.json then
    headers["Accept"] = "application/json"
  end
  if o.referer ~= false then
    local ref = o.referer
    if ref == nil then
      ref = BASE .. "/"
    end
    if ref ~= "" then
      headers["Referer"] = ref
    end
  end
  if type(o.headers) == "table" then
    for k, v in pairs(o.headers) do
      headers[k] = v
    end
  end
  local res, err = host.http.fetch(url, { headers = headers })
  if err ~= nil then
    return nil
  end
  if res.status ~= 200 then
    return nil
  end
  return res.body
end

-- ""/nil -> "unknown", else the host canonicaliser
function M.normalizeStatus(s)
  if not s or s == "" then
    return "unknown"
  end
  return host.text.normalize_status(s)
end

return M
