-- mangaforfree: handler wiring after behavior-preserving split into per-action modules.
local methods = {
  search = require("search"),
  detail = require("detail"),
  chapters = require("chapters"),
  pages = require("pages"),
  genres = require("genres"),
}

function handle(req)
  local fn = methods[req.method]
  if not fn then
    return { ok = false, error = "mangaforfree: unknown method " .. tostring(req.method) }
  end
  local ok, res = pcall(fn, req.params or {})
  if not ok then
    return { ok = false, error = "mangaforfree: " .. tostring(res) }
  end
  return res
end
