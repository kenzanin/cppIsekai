-- dle.lua — shared DataLife Engine guard + payload helpers (plugin-host-promotion
-- D8). Lives in plugins_lib; require()d by every DLE-family plugin (batcave,
-- mangabay) through the read-only lib fallback. Port of the goIsekai helpers;
-- cppIsekai adaptations: requests go through host.http.fetch with cookie=true
-- (the __guard_* trust cookie needs the per-plugin jar; goIsekai's jar was
-- always-on) and host.re2.* replaces host.regex.* (rows are {full, g1, ...}).

local dle = {}

-- Headers sent on behalf of the plugin (flat map for fetch opts.headers).
dle.UA = "Mozilla/5.0 (X11; Linux x86_64; rv:132.0) Gecko/20100101 Firefox/132.0"
dle.ACCEPT_LANG = "en-US,en;q=0.9"

function dle.headers(extra)
    local h = { ["User-Agent"] = dle.UA, ["Accept-Language"] = dle.ACCEPT_LANG }
    if extra then
        for k, v in pairs(extra) do
            h[k] = v
        end
    end
    return h
end

-- pow_nonce finds a nonce whose sha256(token..":"..nonce) has a hex prefix
-- of "00", the difficulty the DLE guard checks. The token is the raw string
-- from the page (url-decoded tokens do NOT match).
function dle.pow_nonce(token)
    if not token then return nil end
    for i = 0, 60000 do
        local h = host.crypto.sha256_hex(token .. ":" .. i)
        if h:sub(1, 2) == "00" then return i, h end
    end
    return nil
end

-- solve_guard runs the handshake: GET target -> 404 challenge page -> pow ->
-- POST /_v -> trust cookie lands in the per-plugin jar. Returns true when the
-- challenge was recognised and (apparently) solved.
function dle.solve_guard(site, body)
    -- pow_nonce only set on the challenge page; a normal 200 carries no
    -- "pow_nonce" token, so this is a no-op there.
    if not body or not body:find("pow_nonce", 1, true) then return false end
    local m = host.re2.find([[token:\s*"([^"]+)"]], body)
    if #m < 2 or m[2] == "" then return false end
    local tok = m[2]
    local nonce, hash = dle.pow_nonce(tok)
    if not nonce then return false end
    local form = "token=" .. host.url_encode(tok)
        .. "&mode=modern&workTime=55&iterations=110&hasCrypto=1&gms=-1&fi=1&rtc=98"
        .. "&rtip=" .. dle.client_ip()
        .. "&pow_nonce=" .. nonce .. "&pow_hash=" .. hash
        .. "&webdriver=0&touch=1&screen_w=1856&screen_h=1044&screen_cd=24"
        .. "&wgv=AMD&wgr=Radeon&tz=-420&dpr=1&cdp=0&cdpf="
    -- Referer/Origin mirror the form the browser sends; an empty rtip
    -- escalates to Turnstile, so the stable placeholder from client_ip rides along.
    host.http.fetch(site .. "/_v", {
        method = "POST",
        body = form,
        cookie = true,
        headers = dle.headers({ Origin = site }),
    })
    return true
end

-- client_ip is a placeholder the server does not actually validate: any
-- non-empty value is accepted, but an empty rtip escalates to Turnstile.
-- A per-plugin "rtip" param overrides the stable default (mangabay union).
function dle.client_ip()
    local ok, ip = pcall(function() return host.plugin.param("rtip") end)
    if ok and ip and ip ~= "" then return ip end
    return "1.2.3.4"
end

-- get fetches site .. path and solves the guard if the response is the
-- 404 challenge page, returning the real body on the second attempt.
function dle.get(site, path)
    local h = { cookie = true, headers = dle.headers() }
    local res, err = host.http.fetch(site .. path, h)
    local body = (err == nil and res and res.body) or ""
    if dle.solve_guard(site, body) then
        res, err = host.http.fetch(site .. path, h)
        body = (err == nil and res and res.body) or ""
    end
    return body
end

-- json_blob: host.text.json_blob passthrough -- first balanced {}/[] after
-- marker as a STRING (D5); nil when no balanced blob follows.
function dle.json_blob(text, marker)
    return host.text.json_blob(tostring(text or ""), marker or "")
end

-- json_blob_decoded: marker -> blob -> decoded table, nil when absent.
function dle.json_data(html, marker)
    local blob = dle.json_blob(html, marker)
    if not blob then return nil end
    local ok, data = pcall(json.decode, blob)
    if not ok or type(data) ~= "table" then return nil end
    return data
end

-- dle_date reformats "18.10.2023" (d.m.y) to the ISO Z form (batcave
-- contract); the host date parser has no dotted layout among its formats.
function dle.dle_date(s)
    if not s then return nil end
    local d, m, y = string.match(tostring(s), "^(%d+)%.(%d+)%.(%d+)")
    if not y then return nil end
    return string.format("%04d-%02d-%02dT00:00:00Z", tonumber(y), tonumber(m), tonumber(d))
end

-- dle_epoch: same dotted date as unix midnight seconds (mangabay contract;
-- Hinnant civil-days, the sandbox has no os.date). 0 when absent/malformed.
function dle.dle_epoch(s)
    local d, m, y = string.match(tostring(s or ""), "^(%d+)%.(%d+)%.(%d+)")
    if not y then return 0 end
    d, m, y = tonumber(d), tonumber(m), tonumber(y)
    local yy = y - (m <= 2 and 1 or 0)
    local era = math.floor(yy / 400)
    local yoe = yy - era * 400
    local doy = math.floor((153 * (m + (m > 2 and -3 or 9)) + 2) / 5) + d - 1
    local doe = yoe * 365 + math.floor(yoe / 4) - math.floor(yoe / 100) + doy
    return (era * 146097 + doe - 719468) * 86400
end

-- slug_id pulls the numeric id and slug out of a DLE permalink
-- (e.g. https://site/x/29706-thor-1966-2011.html -> 29706, thor-1966-2011).
function dle.slug_id(href)
    if not href then return nil, nil end
    local id, slug = string.match(tostring(href), "/(%d+)-([^/]-)%.html")
    return id and tonumber(id), slug
end

-- comic_series finds the ComicSeries node in a JSON-LD @graph block.
function dle.comic_series(html)
    local blocks = host.re2.find_all([[(?s)<script type="application/ld%+json">(.-)</script>]], html or "")
    for _, row in ipairs(blocks) do
        local ok, doc = pcall(json.decode, row[2] or "")
        if ok and type(doc) == "table" and type(doc["@graph"]) == "table" then
            for _, node in ipairs(doc["@graph"]) do
                if type(node) == "table" and node["@type"] == "ComicSeries" then
                    return node
                end
            end
        end
    end
    return nil
end

return dle
