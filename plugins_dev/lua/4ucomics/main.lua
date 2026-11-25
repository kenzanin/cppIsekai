-- 4ucomics: handler wiring after behavior-preserving split into per-action modules.
local methods = {
  search = require("search"),
  detail = require("detail"),
  chapters = require("chapters"),
  pages = require("pages"),
}

function handle(req)
  local fn = methods[req.method]
  if fn == nil then
    return { ok = false, error = "unknown method " .. tostring(req.method) }
  end
  return fn(req.params or {})
end
