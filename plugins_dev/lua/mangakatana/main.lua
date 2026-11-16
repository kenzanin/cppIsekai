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

local function http_req(url)
  local res, err = host.http.fetch(url, opts)
  if err ~= nil then return nil, err end
  if res.status ~= 200 then return nil, "mangakatana: upstream status " .. tostring(res.status) end
  return res.body, nil
end

-- first capture group of the first match; nil when the pattern does not match.
local function find1(s, pattern)
  local m = host.re2.find(pattern, tostring(s or ""))
  if #m >= 2 then return m[2] end
  if #m == 1 then return m[1] end
  return nil
end

-- "https://mangakatana.com/manga/naruto.1205" and "/manga/naruto.1205" both
-- reduce to "naruto.1205"; ids paste straight into host URL paths.
local function slug_of(id)
  return find1(id, [[/manga/([^/?#]+)]]) or id -- absolute URL or clean slug
end

local function url_for(kind, id)
  if #host.re2.find("^https?://", id) > 0 then return id end
  return BASE .. "/" .. kind .. "/" .. id
end

-- "naruto.1205:c700" -> the chapter's absolute URL.
local function chapter_url(id)
  if #host.re2.find("^https?://", id) > 0 then return id end
  local slug, tail = id:match("^([^:]+):(.+)$")
  if not slug then return id end
  return url_for("manga", slug) .. "/" .. tail
end

-- The reader page ships no image URLs in its markup: every <img> carries
-- data-src="#" and a load handler copies the real URLs out of a JavaScript
-- array (var thzq=[...]) into them. No selector can read that, so read the
-- array — the longest one on the page wins, which keeps a rename of the
-- obfuscated variable from breaking us.
local function page_urls(html)
    local best = {}
    for _, m in ipairs(host.re2.find_all([=[(?s)\[(.*?)\]]=], html)) do
        local urls = {}
        for _, u in ipairs(host.re2.find_all([=['](https?://[^']+)']=], m[2])) do
            urls[#urls + 1] = u[2]
        end
        if #urls > #best then best = urls end
    end
    return best
end

local methods = {}

-- ── search ─────────────────────────────────────────────────────────────────

methods.search = function(params)
  local q = params.title or params.query or params.q or ""
  local page = math.max(1, tonumber(params.page) or 1)
  local genres = params.tags or params.genres or {}
  if type(genres) == "string" then
    local list = {}
    for name in genres:gmatch("[^,]+") do list[#list + 1] = host.text.trim(name) end
    genres = list
  end

  local url
  if type(genres) == "table" and #genres > 0 then
    -- Genre browsing: /genre/<slug>; page 2+ moves the path to /page/N. The
    -- archive reuses the search card layout, so one parser serves both.
    local slug = require("genres").map[genres[1]] or genres[1]
    url = BASE .. "/genre/" .. host.url_encode(slug)
    if page > 1 then url = url .. "/page/" .. page end
  else
    -- Page 1 is the bare query string; later pages /page/N?...
    url = BASE .. "/?search=" .. host.url_encode(q) .. "&search_by=book_name"
    if page > 1 then
      url = BASE .. "/page/" .. page .. "?search=" .. host.url_encode(q) .. "&search_by=book_name"
    end
  end

  local body, err = http_req(url)
  if err ~= nil then return { ok = false, error = err } end

  -- Every card carries a title link and one cover <img> (results without a
  -- cover use /imgs/no-cover.png), so the lists stay index-aligned.
  local hrefs = host.html.attr(body, "#book_list .item h3.title a", "href")
  local titles = host.html.select(body, "#book_list .item h3.title a")
  local covers = host.html.attr(body, "#book_list .item .media img", "src")

  local items, seen = {}, {}
  for i, href in ipairs(hrefs) do
    local slug = slug_of(href)
    if slug and not seen[slug] then
      seen[slug] = true
      items[#items + 1] = { id = slug, name = host.text.unescape(titles[i] or ""), cover = covers[i] or "" }
    end
  end
  return { ok = true, data = { items = items, total = #items } }
end

-- ── detail ─────────────────────────────────────────────────────────────────

methods.detail = function(params)
  local manga_id = slug_of(params.id or "")
  local empty = { id = manga_id, name = "", cover = "", status = "unknown", tags = {}, release_at = 0,
    description = "", authors = {}, artists = {}, url = url_for("manga", manga_id) }

  local body, err = http_req(url_for("manga", manga_id))
  if err ~= nil then return { ok = false, error = err } end

  -- #related sits inside #single_book and repeats the card shape, so every
  -- lookup below is scoped to stay off it.
  local tags = {}
  for _, name in ipairs(host.html.select(body, "#single_book .genres a")) do
    tags[#tags + 1] = host.text.trim(name)
  end

  -- The description is one <p> whose line breaks are <br> tags; the text-node
  -- xpath splits exactly where the <br>s are (find_text would glue the words).
  local description = table.concat(
    host.html.xpath("//div[@id='single_book']//div[@class='summary']/p/text()", body) or {}, "\n")

  local data = empty
  data.name = host.html.select(body, "h1.heading")[1] or ""
  local author = host.html.select(body, "#single_book .authors a.author")[1]
  data.cover = host.html.attr(body, "#single_book .cover img", "src")[1] or ""
  -- The status cell reads "Completed" / "Ongoing" / "Cancelled"; the host
  -- owns the canonical vocabulary.
  data.status = host.text.normalize_status(host.html.select(body, "#single_book div.value.status")[1] or "")
  data.tags = tags
  data.description = description
  if author and author ~= "" then data.authors = { host.text.trim(author) } end
  return { ok = true, data = data }
end

-- ── chapters ───────────────────────────────────────────────────────────────

methods.chapters = function(params)
  local manga_id = slug_of(params.id or "")
  if manga_id == "" then return { ok = true, data = {} } end

  local body, err = http_req(url_for("manga", manga_id))
  if err ~= nil then return { ok = false, error = err } end

  local selector = "div.chapters table tbody tr td div.chapter a"
  local urls = host.html.attr(body, selector, "href")
  local titles = host.html.select(body, selector)

  -- The table is newest-first already, which is the ABI convention: keep order.
  local out = {}
  for i, href in ipairs(urls) do
    -- .../c247, .../c230.5 and .../c0-v2 all end in the segment to keep.
    local tail = find1(href, [[/([^/]+)$]]) or href
    out[#out + 1] = {
      id = manga_id .. ":" .. tail,
      -- The number is the tail of the href: .../c247, .../c230.5, .../c0-v2
      number = tonumber(find1(href, [[/c([\d.]+)]])) or 0,
      title = host.text.trim(titles[i] or ""),
      release_at = 0, -- page shows no machine dates
      pages = 0,
    }
  end
  return { ok = true, data = out }
end

-- ── pages ──────────────────────────────────────────────────────────────────

methods.pages = function(params)
  local body, err = http_req(chapter_url(params.id or ""))
  if err ~= nil then return { ok = false, error = err } end

  local urls = page_urls(body)
  if #urls == 0 then return { ok = false, error = "mangakatana: no pages for " .. tostring(params.id) } end
  return { ok = true, data = urls }
end

-- ── genres ─────────────────────────────────────────────────────────────────

methods.genres = function()
  return { ok = true, data = require("genres").names }
end

function handle(req)
  local fn = methods[req.method]
  if fn == nil then return { ok = false, error = "unknown method " .. tostring(req.method) } end
  return fn(req.params or {})
end
