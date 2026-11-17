-- scratch: in-daemon probe lane. eval = browser; raw = plain http fetch + regex census.
local methods = {}

methods.raw = function(params)
  local url = params.url or ""
  local res, err = host.http.fetch(url)
  if err ~= nil then return { ok = false, error = "fetch: " .. err } end
  local body = res.body or ""
  local function cnt(p)
    local n = 0
    for _ in ipairs(host.re2.find_all(p, body)) do n = n + 1 end
    return n
  end
  local run = body:match("ts_reader%.run%((.*)%)") or ""
  local ok_c, capres = pcall(function()
    return host.re2.match([[(?s)ts_reader\.run\((\{.*?\})\)]], body)
  end)
  local run2 = type(capres) == "table" and (capres[2] or capres[1]) or ""
  local img_ok, imgb = pcall(function()
    return host.re2.match([=[(?s)"images"\s*:\s*\[(.*?)\]]=], run2 ~= "" and run2 or body)
  end)
  local imgblock = type(imgb) == "table" and (imgb[2] or imgb[1]) or ""
  return { ok = true, data = {
    status = res.status,
    len = #body,
    run_lua = #run,
    cap_type = type(capres),
    cap_n = type(capres) == "table" and #capres or 0,
    run2_len = #run2,
    img_type = type(imgb),
    img_n = type(imgb) == "table" and #imgb or 0,
    img_len = #imgblock,
    img_head = imgblock:sub(1, 80),
  } }
end

function handle(req)
  local fn = methods[req.method]
  if fn == nil then return { ok = false, error = "unknown method " .. tostring(req.method) } end
  return fn(req.params or {})
end
