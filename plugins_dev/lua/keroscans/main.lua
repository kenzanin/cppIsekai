-- KeroScans (https://keroscans.net) plugin, api 1. Custom theme, bukan Madara:
-- /search?q= grid artikel; series page embed chapter page 1, older pages via
-- ?page=N&partial=chapters; reader images /media/pages/* same-host.
-- Split per action; file ini cuma wiring (pola 1manga).
local BASE = host.plugin.param("site_url") or "https://keroscans.net"

local http = require("http")
local ctx = {
  BASE = BASE,
  get = http.get,
}

local methods = {
  search = require("search")(ctx),
  detail = require("detail")(ctx),
  chapters = require("chapters")(ctx),
  pages = require("pages")(ctx),
  genres = function()
    local g = require("genres").load(ctx)
    return { ok = true, data = g.names }
  end,
}

function handle(req)
  local fn = methods[req.method]
  if fn == nil then
    return { ok = false, error = "unknown method " .. tostring(req.method) }
  end
  return fn(req.params or {})
end
