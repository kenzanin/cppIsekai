-- scratch: in-daemon probe lane. raw = impersonated GET, prints status+len+head.
local methods = {}

methods.raw = function(params)
  local url = params.url or ""
  local res, err = host.http.fetch(url)
  if err ~= nil then return { ok = false, error = "fetch: " .. err } end
  local body = res.body or ""
  return { ok = true, data = { status = res.status, len = #body, head = body:sub(1, 300) } }
end

function handle(req)
  local fn = methods[req.method]
  if fn == nil then return { ok = false, error = "unknown method " .. tostring(req.method) } end
  return fn(req.params or {})
end
