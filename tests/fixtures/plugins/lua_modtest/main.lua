local util = require("lib.util")
function handle(req)
  if req.method == "ping" then
    return {ok=true, data={doubled=util.double(21), msg=util.MSG}}
  end
  return {ok=false, error="unknown method"}
end
