function handle(req)
  local res, err = host.http.fetch(req.params.url, req.params.opts)
  if err ~= nil then return {ok=false, error=err} end
  if res.status ~= 200 then return {ok=false, error="status " .. res.status} end
  return {ok=true, data={status=res.status, body=res.body, has_headers=res.headers ~= nil}}
end
