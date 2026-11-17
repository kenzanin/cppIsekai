-- scratch: in-daemon browser probe (runs inside the daemon's lightpanda
-- session via host.browser). Diagnostic lane for the comix WAF hunt.
local methods = {}

methods.eval = function(params)
  local url = params.url or "https://comix.to/"
  local script = params.script or "document.title"
  local tmo = tonumber(params.tmo) or 0
  local _, gerr = host.browser.call("goto", { url = url, timeout_ms = tmo > 0 and tmo or nil })
  if gerr ~= nil then return { ok = false, error = "goto: " .. gerr } end
  local out, eerr = host.browser.call("evaluate", { script = script, timeout_ms = tmo > 0 and tmo or nil })
  if eerr ~= nil then return { ok = false, error = "eval: " .. eerr } end
  return { ok = true, data = out }
end

function handle(req)
  local fn = methods[req.method]
  if fn == nil then return { ok = false, error = "unknown method " .. tostring(req.method) } end
  return fn(req.params or {})
end
