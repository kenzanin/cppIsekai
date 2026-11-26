-- HariManga (harimanga.cc): WordPress Madara theme, HTML scraping; Cloudflare-fronted.
-- Port of goIsekai examples/plugins/lua/harimanga; adapted to cppIsekai host tools
-- (host.http.fetch replaces host.http.get_body/post_body, host.re2.* replaces
-- host.regex.* with arg order pattern,subject). Envelope { ok, data | error };
-- canonical manga fields per the mangadex plugin.
local BASE = "https://www.harimanga.cc" -- single origin; manifest site_url kept verbatim in main.json

-- http request -> (body, err) via the host browser (CF challenge zone:
-- chrome116 curl eats 403; goto solves it, then read outerHTML -- comix pattern).
local function http_req(url)
  local _, gerr = host.browser.call("goto", { url = url, timeout_ms = 60000 })
  if gerr ~= nil then
    return nil, "harimanga: goto failed: " .. gerr
  end
  -- the challenge page answers navigation instantly; wait it out or we
  -- evaluate "Just a moment..." and parse zero items silently
  local _, werr = host.browser.call(
    "wait",
    { expression = "document.title.indexOf('Just a moment')===-1", timeout_ms = 20000 }
  )
  if werr ~= nil then
    return nil, "harimanga: challenge stuck on " .. url
  end
  local body, eerr = host.browser.call("evaluate", { script = "document.documentElement.outerHTML", timeout_ms = 30000 })
  if eerr ~= nil or body == nil or body == "" then
    return nil, "harimanga: render failed: " .. tostring(eerr)
  end
  return body, nil
end

-- first capture group of the first match; nil when no match (host re2.capture).
local function find1(s, pattern)
  return host.re2.capture(pattern, tostring(s or ""))
end

-- label -> url-slug map, filled by methods.genres; search resolves UI names.
-- Shared table on purpose: genres.lua mutates this same reference.
local genre_map = {}

return { BASE = BASE, http_req = http_req, genre_map = genre_map, find1 = find1 }
