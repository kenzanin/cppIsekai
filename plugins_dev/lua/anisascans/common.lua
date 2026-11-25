-- AnisaScan plugin (anisascans.in): WordPress Madara theme, HTML scraping.
-- Port of goIsekai examples/plugins/lua/anisascans; adapted to cppIsekai host tools
-- (host.http.fetch replaces goIsekai host.http.get_body; host.re2.find replaces
-- host.regex.find; see references/goisekai-abi-mapping for the full gap list).
-- Envelope { ok, data | error }; canonical manga fields per the mangadex plugin.
local BASE = "https://anisascans.com" -- .in died (NXDOMAIN); .com is live

-- ── helpers (host delegation, plugin-host-promotion) ────────────────────

local trim = host.text.trim
local unescape = host.text.unescape
local strip_html = host.text.strip_html
local normalize_status = host.text.normalize_status

-- Madara labels ("Chapter 12", "Ch. 12") -> number; 0 when none.

-- http request -> (body, err): the host two-value fetch_body form.
local http_req = host.http.fetch_body

-- every match of `pattern` in `s`, each as a re2 group array (goIsekai regex.gmatch).

-- first capture group of the first match; nil when the pattern does not match.


-- ── search ─────────────────────────────────────────────────────────────────


-- ── detail ─────────────────────────────────────────────────────────────────


-- ── chapters ───────────────────────────────────────────────────────────────


-- ── pages ──────────────────────────────────────────────────────────────────


-- ── genres ─────────────────────────────────────────────────────────────────


local function chapter_num(s)
  s = tostring(s or "")
  local n = s:match("(%d+%.?%d*)%s*$")
  if n then return tonumber(n) end
  n = s:match("[Cc]hapter%s+(%d+%.?%d*)")
  if n then return tonumber(n) end
  return 0
end

local function find_all(s, pattern)
  return host.re2.find_all(pattern, tostring(s or ""))
end

local function find1(s, pattern)
  return host.re2.capture(pattern, tostring(s or ""))
end

return { BASE=BASE, trim=trim, unescape=unescape, strip_html=strip_html, normalize_status=normalize_status, http_req=http_req, chapter_num=chapter_num, find_all=find_all, find1=find1 }
