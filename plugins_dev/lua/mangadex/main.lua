-- mangadex: handler wiring after behavior-preserving split into per-method modules.
local methods = {
  search = require("search"),
  genres = require("genres"),
  detail = require("detail"),
  related = require("related"),
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
