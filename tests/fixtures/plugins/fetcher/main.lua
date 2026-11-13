function handle(req)
  if req.method == "get_json" then
    local res = host.http.get_json(req.params.url, req.params.headers)
    return {ok = true, data = res}
  end
  if req.method == "post_json" then
    local res = host.http.post_json(req.params.url, req.params.body or "", req.params.headers)
    return {ok = true, data = res}
  end
  local res, err = host.http.fetch(req.params.url, req.params.opts)
  if err ~= nil then return {ok=false, error=err} end
  if res.status ~= 200 then return {ok=false, error="status " .. res.status} end
  return {ok=true, data={status=res.status, body=res.body, has_headers=res.headers ~= nil}}
end
