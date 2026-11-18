-- throwaway probe: batcave poster region (method detail)
local function probe(params)
    local body, err = host.http.fetch_body("https://batcave.cc/8413-arthur-sex.html")
    host.log.info("PROBE fetch=" .. tostring(body and #body or "nil") .. " err=" .. tostring(err))
    if body then
        local i = body:find("page__poster") or 0
        host.log.info("PROBE at=" .. i)
        if i > 0 then host.log.info("PROBE region: " .. body:sub(math.max(1, i - 150), i + 300):gsub("[\n\r]", " ")) end
    end
    return { ok = true, data = { len = body and #body or 0 } }
end
function handle(req)
    if req.method == "detail" then return probe(req.params or {}) end
    return { ok = false, error = "unknown method " .. tostring(req.method) }
end
