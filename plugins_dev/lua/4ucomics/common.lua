-- 4ucomics: private JSON API v1 behind Cloudflare (the HTML shell is a JS
-- SPA). API calls ride the lightpanda renderer profile -- the challenge
-- solver (decisions/renderer-fetch-engines). Page/cover CDNs vary per title
-- and some (mangadot.net) sit behind CF too: every image URL is wrapped in
-- the site's own /api/v1/img proxy, fetched by the host with the manifest
-- `image_engine` impersonation profile (image-engine-profile).
-- Envelope { ok, data | error }; canonical manga fields per the mangafire plugin.
local API = "https://4ucomics.com/api/v1"


-- Site vocab (anilist-style) -> canonical lowercase; unknown falls to "unknown".
local STATUS = {
  RELEASING = "ongoing", ONGOING = "ongoing",
  FINISHED = "completed", COMPLETED = "completed",
  HIATUS = "hiatus",
  CANCELLED = "dropped", DROPPED = "dropped",
  NOT_YET_RELEASED = "upcoming", UPCOMING = "upcoming",
}









local function proxy_img(u)
  if type(u) ~= "string" or u == "" then return u end
  return API .. "/img?url=" .. host.url_encode(u)
end

local function status_of(list)
  for _, s in ipairs(list or {}) do
    local m = STATUS[string.upper(tostring(s))]
    if m then return m end
  end
  return "unknown"
end

local function get_json(path)
  local body, ferr = host.http.fetch_body(API .. path, { profile = "lightpanda", timeout_ms = 60000 })
  if body == nil then return nil, ferr end
  if body:sub(1, 1) == "<" then body = body:match("<pre>(.*)</pre>") or body end -- renderer wraps json in <pre>
  local ok, data = pcall(json.decode, body)
  if not ok or type(data) ~= "table" then return nil, "4ucomics: invalid JSON" end
  return data, nil
end

local function number_of(name)
  local n = host.text.chapter_num(tostring(name or ""))
  if n == nil or n ~= n then return 0 end
  return n
end

return { API=API, STATUS=STATUS, proxy_img=proxy_img, status_of=status_of, get_json=get_json, number_of=number_of }
