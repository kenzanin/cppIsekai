-- plugin-host-promotion 6.2: require() shared-lib fallback, local shadow,
-- traversal rejection and the not-found message, exercised through handle().
local shared = require("sharedtags")     -- only in fixtures/plugins_lib
local shadow = require("shadowed")       -- plugin-local lib shadows the lib copy
local absent = pcall(require, "nope_nope")
local oktrav, errtrav = pcall(require, "../secrets")

function handle(req)
  if req.method == "ping" then
    local ok2, err = pcall(require, "nope_nope") -- require raises; capture via pcall
    err = ok2 and "no-error" or tostring(err) -- real error return for the message check
    return { ok = true, data = {
      shared_tag = shared.TAG,
      shadow_tag = shadow.TAG,
      traversal_ok = absent,
      trav_err = tostring(errtrav),
      miss = tostring(err),
    } }
  end
  return { ok = false, error = "unknown method" }
end
