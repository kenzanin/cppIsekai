-- throwaway probe: comico reader open raw status per chapter id
local SITE = "https://comico.moe"
local API = SITE .. "/api"
local UA = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36"
local function probe(params)
    local cid = params.id or ""
    local out = {}
    local a, aerr = host.http.fetch(API .. "/reader/access/" .. host.url_encode(cid), { headers = { ["User-Agent"] = UA } })
    out.access_st = a and a.status or tostring(aerr)
    local token = ""
    local aj = host.http.get_json(API .. "/reader/access/" .. host.url_encode(cid))
    token = aj and aj.token or "none"
    local r, e = host.http.fetch(API .. "/reader/chapters/" .. host.url_encode(cid) .. "/open?d=" .. host.url_encode(token),
        { method = "POST", body = '{"proxyMethod":"all"}', headers = { ["User-Agent"] = UA, ["Content-Type"] = "application/json", ["Referer"] = SITE .. "/" } })
    out.post_st = r and r.status or tostring(e)
    out.body = r and (r.body or ""):sub(1, 150) or ""
    host.log.info("PROBE " .. cid .. " access=" .. tostring(out.access_st) .. " post=" .. tostring(out.post_st) .. " body=" .. out.body)
    return { ok = true, data = out }
end
function handle(req)
    if req.method == "detail" then return probe(req.params or {}) end
    return { ok = false, error = "unknown method " .. tostring(req.method) }
end
