function handle(req)
  if req.method == "ping" then
    return {ok=true, data={pong=true, api=req.api, echo=req.params.echo}}
  elseif req.method == "fail" then
    return {ok=false, error="boom"}
  elseif req.method == "weird" then
    return 42
  elseif req.method == "no_ok" then
    return {data="x"}
  elseif req.method == "log" then
    host.log.info("good plugin says hi")
    return {ok=true, data={t=host.now()}}
  end
  return {ok=false, error="unknown method"}
end
