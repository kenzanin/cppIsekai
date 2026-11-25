-- Comix.to plugin (https://comix.to): Nuxt SSR + SPA reader. Port of goIsekai
-- examples/plugins/lua/comix. Chapters/pages harvest through the host browser
-- bridge; browse/detail use the render profile. Envelope { ok, data | error }.
local methods = {
  search = require("search"),
  detail = require("detail"),
  chapters = require("chapters"),
  pages = require("pages"),
  genres = require("genres"),
}

function handle(req)
  local fn = methods[req.method]
  if fn == nil then return { ok = false, error = "unknown method " .. tostring(req.method) } end
  return fn(req.params or {})
end
