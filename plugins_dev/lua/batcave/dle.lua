-- dle.lua — DLE (DataLifeEngine) guard + page-shape helpers.
-- Port of the goIsekai batcave helper; cppIsekai adaptations: requests go
-- through host.http.fetch with cookie=true (the __guard_* trust cookie needs
-- the per-plugin jar; goIsekai's jar was always-on), host.text.json_blob does
-- not exist here so the balanced-brain scan lives below, and host.re2.*
-- replaces host.regex.* (result rows are {full, g1, ...}).

local dle = {}

-- Headers sent on behalf of the plugin (flat map for fetch opts.headers).
dle.UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36"
dle.ACCEPT_LANG = "en-US,en;q=0.9"

function dle.headers(extra)
    local h = { ["User-Agent"] = dle.UA, ["Accept-Language"] = dle.ACCEPT_LANG }
    if extra then
        for k, v in pairs(extra) do h[k] = v end
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
        if string.sub(h, 1, 2) == "00" then
            return i, h
        end
    end
    return nil
end

-- solve_guard runs the handshake: GET target -> 404 challenge page -> pow ->
-- POST /_v -> trust cookie lands in the per-plugin jar. Returns true when the
-- challenge was recognised and (apparently) solved.
function dle.solve_guard(site, body)
    -- pow_nonce only set on the challenge page; a normal 200 carries no
    -- "pow_nonce" token, so this is a no-op there.
    if not body or not string.find(body, "pow_nonce", 1, true) then
        return false
    end
    local m = host.re2.find([[token:\s*"([^"]+)"]], body)
    if #m < 2 then return false end
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
function dle.client_ip()
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
    return body, res
end

-- json_blob returns the first balanced JSON object or array embedded in text,
-- starting at the first occurrence of marker ("" searches from the start).
-- Quotes and escapes are tracked, so a brace inside a string never closes the
-- blob early. Returns nil when no balanced blob follows the marker.
function dle.json_blob(text, marker)
    if marker and marker ~= "" then
        local i = string.find(text, marker, 1, true)
        if not i then return nil end
        text = string.sub(text, i + #marker)
    end
    local start = string.find(text, "[{[]")
    if not start then return nil end
    local open, closer = string.sub(text, start, start), "}"
    if open == "[" then closer = "]" end
    local depth, in_string, escaped = 0, false, false
    for i = start, #text do
        local c = string.sub(text, i, i)
        if in_string then
            if escaped then
                escaped = false
            elseif c == "\\" then
                escaped = true
            elseif c == '"' then
                in_string = false
            end
        else
            if c == '"' then
                in_string = true
            elseif c == open then
                depth = depth + 1
            elseif c == closer then
                depth = depth - 1
                if depth == 0 then return string.sub(text, start, i) end
            end
        end
    end
    return nil
end

-- json_blob_decoded: marker -> blob -> decoded table, nil when absent.
function dle.json_data(html, marker)
    local blob = dle.json_blob(html, marker)
    if not blob then return nil end
    local ok, data = pcall(json.decode, blob)
    if not ok or type(data) ~= "table" then return nil end
    return data
end

-- dle_date reformats "18.10.2023" (d.m.yyyy) to ISO, because the host
-- date parser has no dotted layout among its accepted formats.
function dle.dle_date(s)
    if not s then return nil end
    local d, m, y = string.match(s, "^(%d+)%.(%d+)%.(%d+)")
    if not y then return nil end
    return string.format("%04d-%02d-%02dT00:00:00Z", tonumber(y), tonumber(m), tonumber(d))
end

-- slug_id pulls the numeric id and slug out of a DLE permalink
-- (e.g. https://site/x/29706-thor-1966-2011.html -> 29706, thor-1966-2011).
function dle.slug_id(href)
    if not href then return nil, nil end
    local id, slug = string.match(href, "/(%d+)-([^/]-)%.html")
    return id and tonumber(id), slug
end

-- comic_series finds the ComicSeries node in a JSON-LD @graph block.
function dle.comic_series(html)
    local blocks = host.re2.find_all([[(?s)<script type="application/ld\+json">(.*?)</script>]], html)
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
