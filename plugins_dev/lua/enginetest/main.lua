-- throwaway probe: host.text.unescape entity behavior
local function probe(_)
    local out = {
        num039 = host.text.unescape("A Man&#039;s X"),
        num38 = host.text.unescape("a &#38; b"),
        named = host.text.unescape("a &amp; b &lt; c"),
        hex = host.text.unescape("x&#x27;y"),
        plain = host.text.unescape("no entities here"),
        empty = host.text.unescape(""),
    }
    host.log.info("UNESCAPE " .. tostring(out.num039) .. " | " .. tostring(out.named))
    return { ok = true, data = out }
end
function handle(req)
    if req.method == "detail" then return probe(req.params or {}) end
    return { ok = false, error = "unknown method " .. tostring(req.method) }
end
