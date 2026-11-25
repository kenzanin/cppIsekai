-- MangaKatana plugin (mangakatana.com): custom theme, CSS-selector scraping.
-- Port of goIsekai examples/plugins/lua/mangakatana; host.html.find_* on a
-- parsed doc become host.html.select/attr on raw HTML, the description xpath
-- maps to host.html.xpath. Envelope { ok, data | error }.
local BASE = "https://mangakatana.com"

local opts = { -- static theme wants a browser-ish UA on every request
  headers = {
    ["User-Agent"] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
    ["Accept"] = "text/html,application/xhtml+xml",
  },
}

-- ── helpers ────────────────────────────────────────────────────────────────


-- first capture group of the first match; nil when no match (host re2.capture).

-- "https://mangakatana.com/manga/naruto.1205" and "/manga/naruto.1205" both
-- reduce to "naruto.1205"; ids paste straight into host URL paths.


-- "naruto.1205:c700" -> the chapter's absolute URL.

-- The reader page ships no image URLs in its markup: every <img> carries
-- data-src="#" and a load handler copies the real URLs out of a JavaScript
-- array (var thzq=[...]) into them. No selector can read that, so read the
-- array — the longest one on the page wins, which keeps a rename of the
-- obfuscated variable from breaking us.


-- ── search ─────────────────────────────────────────────────────────────────


-- ── detail ─────────────────────────────────────────────────────────────────


-- ── chapters ───────────────────────────────────────────────────────────────


-- ── pages ──────────────────────────────────────────────────────────────────


-- ── genres ─────────────────────────────────────────────────────────────────


local function http_req(url)
  return host.http.fetch_body(url, opts) -- (body, err); UA kept: static theme wants a browser-ish UA (plugin-host-promotion).
end

local function find1(s, pattern)
  return host.re2.capture(pattern, tostring(s or ""))
end

local function slug_of(id)
  return find1(id, [[/manga/([^/?#]+)]]) or id -- absolute URL or clean slug
end

local function url_for(kind, id)
  if #host.re2.find("^https?://", id) > 0 then return id end
  return BASE .. "/" .. kind .. "/" .. id
end

local function chapter_url(id)
  if #host.re2.find("^https?://", id) > 0 then return id end
  local slug, tail = id:match("^([^:]+):(.+)$")
  if not slug then return id end
  return url_for("manga", slug) .. "/" .. tail
end

local function page_urls(html)
    local best = {}
    for _, m in ipairs(host.re2.find_all([=[(?s)\[(.*?)\]]=], html)) do
        local urls = {}
        for _, u in ipairs(host.re2.find_all("'(https?://[^']+)'", m[2])) do
            urls[#urls + 1] = u[2]
        end
        if #urls > #best then best = urls end
    end
    return best
end

return { BASE=BASE, opts=opts, http_req=http_req, find1=find1, slug_of=slug_of, url_for=url_for, chapter_url=chapter_url, page_urls=page_urls }
