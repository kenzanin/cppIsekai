-- MadaraDex (madaradex.org): WordPress Madara theme, manga under /title/;
-- the chapter list is inline on the detail page (MangaPage mode, no ajax).
-- Port of goIsekai examples/plugins/lua/madaradex; host.re2.* replaces
-- host.regex.* (pattern-first args), fetch+per-plugin cookie jar replaces
-- get_body/post_body (the site refreshes an mdx_auth cookie via admin-ajax
-- and expects it on every later request -> cookie=true everywhere).
-- Envelope { ok, data | error }; canonical manga fields per the mangadex plugin.
local BASE = "https://madaradex.org" -- manifest site_url
local AJAX = BASE .. "/wp-admin/admin-ajax.php"
local MANGA_PATH = "/title/"

-- One handshake per load: MadaraDex intercepts requests while mdx_auth is
-- missing, the POST reply sets it in the per-plugin jar.
local auth_done = false
local function warm_auth()
  if auth_done then
    return
  end
  auth_done = true
  host.http.fetch(AJAX, {
    method = "POST",
    body = "action=mdx_auth_refresh",
    headers = {
      ["X-Mdx-Auth-Refresh"] = "1",
      ["X-Requested-With"] = "xmlhttprequest",
      ["Content-Type"] = "application/x-www-form-urlencoded",
    },
    cookie = true,
  })
end

-- http request -> (body, err); cookie jar on every call (mdx_auth).
local function http_req(url, opts)
  warm_auth()
  local o = {}
  if opts then
    for k, v in pairs(opts) do
      o[k] = v
    end
  end
  o.cookie = true
  return host.http.fetch_body(url, o)
end

-- first capture group of the first match; nil when no match (host re2.capture).
local function find1(s, pattern)
  return host.re2.capture(pattern, tostring(s or ""))
end

-- abs_url: host.url_abs anchored at BASE (D6, this plugin's variant is the
-- canonical semantics).
local abs_url = function(raw)
  return host.url_abs(BASE, raw)
end

local function manga_url(slug)
  return BASE .. MANGA_PATH .. host.url_encode(slug) .. "/"
end

-- Lazy attributes win over src, in the order Madara emits them.
local function img_src(fragment)
  if not fragment or fragment == "" then
    return ""
  end
  local v = find1(fragment, [[data-src="\s*(https?://[^"]+)]])
    or find1(fragment, [[data-src="\s*(/[^"]+)]])
    or find1(fragment, [[data-lazy-src="\s*(https?://[^"]+)]])
    or find1(fragment, [[data-cfsrc="\s*(https?://[^"]+)]])
    or find1(fragment, [[data-manga-src="\s*(https?://[^"]+)]])
    or find1(fragment, [[src="\s*(https?://[^"]+)]])
    or find1(fragment, [[src="\s*(/[^"]+)]])
  if v then
    return abs_url(host.text.trim(v))
  end
  return ""
end

-- name -> slug map filled by methods.genres; search resolves UI genre names.
local genre_map = nil

-- ── search ─────────────────────────────────────────────────────────────────

return {
  BASE = BASE,
  AJAX = AJAX,
  MANGA_PATH = MANGA_PATH,
  auth_done = auth_done,
  warm_auth = warm_auth,
  http_req = http_req,
  find1 = find1,
  abs_url = abs_url,
  manga_url = manga_url,
  img_src = img_src,
  genre_map = genre_map,
}
