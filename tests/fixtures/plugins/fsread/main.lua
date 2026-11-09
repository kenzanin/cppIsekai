function handle(req)
  if req.method == "ping" then
    local c = host.fs.read("main.lua")
    if type(c) ~= "string" then
      return {ok=false, error="no self"}
    end
    return {ok=true, data={len=#c}}
  elseif req.method == "escape" then
    return {ok=true, data={gone=host.fs.read("../nope-escape") == nil and host.fs.read("/etc/hostname") == nil}}
  end
  return {ok=false, error="unknown method"}
end
