-- scratch: fetch-gate engine probe (lightpanda / obscura headless renderers).
-- Lane-scrap; delete with the engine lane once real plugins land.
local methods = {}

local URLS = {
  u4 = "https://4ucomics.com/",
  tk = "https://toonkotsu.com/",
}

local function probe(engine, key)
  local r = host.http.fetch(URLS[key], { profile = engine, timeout_ms = 90000 })
  local title = r.body and r.body:match("<title>([^<]*)") or "none"
  return { status = r.status, bytes = r.body and #r.body or 0, title = title }
end

methods.probe = function(params)
  local engine = params.engine or "lightpanda"
  local key = params.key or "u4"
  local ok, res = pcall(probe, engine, key)
  if not ok then return { ok = false, error = tostring(res) } end
  return { ok = true, data = res }
end

function handle(req)
  local fn = methods[req.method]
  if fn == nil then return { ok = false, error = "unknown method " .. tostring(req.method) } end
  return fn(req.params or {})
end
