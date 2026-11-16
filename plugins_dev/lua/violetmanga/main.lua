-- VioletManga plugin (violetmanga.com): WordPress mangareader/bixbox theme.
-- Port of goIsekai examples/plugins/lua/violetmanga. The source read page lists
-- via host.browser.evaluate (no cppIsekai equivalent); instead the ts_reader
-- SSR "images" array is regex-scanned from the reader HTML — the path the
-- source itself uses against the raw body. Envelope { ok, data | error }.
local BASE = "https://violetmanga.com"

-- ── helpers ────────────────────────────────────────────────────────────────

local function http_req(url)
  local res, err = host.http.fetch(url)
  if err ~= nil then return nil, err end
  if res.status ~= 200 then return nil, "violetmanga: upstream status " .. tostring(res.status) end
  return res.body, nil
end

-- first text node of a selector; "" when absent
local function sel1(html, css)
  local r = host.html.select(html, css)
  return r and r[1] or ""
end

local methods = {}

-- ── search ─────────────────────────────────────────────────────────────────

methods.search = function(params)
  local q = params.title or params.query or params.q or ""
  local page = math.max(1, tonumber(params.page) or 1)
  local url = BASE .. "/?s=" .. host.url_encode(q)
  if page > 1 then url = url .. "&paged=" .. page end
  local body, err = http_req(url)
  if err ~= nil then return { ok = false, error = err } end

  -- bixbox card: .bsx > a (href /comics/slug/) + .tt title + img[src]; the
  -- three queries are index-aligned per card, as in the source.
  local hrefs = host.html.attr(body, ".bsx > a", "href")
  local titles = host.html.select(body, ".bsx .tt")
  local covers = host.html.attr(body, ".bsx > a img", "src")
  local items = {}
  for i, href in ipairs(hrefs) do
    local slug = tostring(href):match("/comics/([^/]+)/?")
    if slug then
      items[#items + 1] = { id = slug, name = titles[i] or "", cover = covers[i] or "" }
    end
  end
  return { ok = true, data = { items = items, total = #items } }
end

-- ── detail ─────────────────────────────────────────────────────────────────

methods.detail = function(params)
  local slug = params.id or ""
  local empty = { id = slug, name = "", cover = "", status = "unknown", tags = {}, release_at = 0,
    description = "", authors = {}, artists = {}, url = BASE .. "/comics/" .. slug .. "/" }
  if slug == "" then return { ok = true, data = empty } end

  local body, err = http_req(BASE .. "/comics/" .. slug .. "/")
  if err ~= nil then return { ok = false, error = err } end

  local data = empty
  data.name = host.text.unescape(sel1(body, "h1.entry-title"))
  local author = host.text.unescape(sel1(body, ".imptdt span.author i"))
  if author ~= "" then data.authors = { author } end
  data.tags = host.html.select(body, ".genres-container .mgen a")
  data.status = host.text.normalize_status(sel1(body, ".info-half .status i"))
  data.cover = host.html.attr(body, ".thumb img", "src")[1] or ""

  -- description block; Lua patterns span newlines natively (no (?s) needed)
  local dblock = body:match('class="summary[^"]*">.-class="entry%-content[^"]*">(.-)</div>')
    or body:match('class="entry%-content[^"]*">(.-)</div>') or ""
  data.description = host.text.unescape(host.text.strip_html(dblock)):gsub("%s+", " ")

  return { ok = true, data = data }
end

-- ── chapters ───────────────────────────────────────────────────────────────

methods.chapters = function(params)
  local slug = params.id or ""
  if slug == "" then return { ok = true, data = {} } end
  local body, err = http_req(BASE .. "/comics/" .. slug .. "/")
  if err ~= nil then return { ok = false, error = err } end

  -- detail-page SSR list; locked (coin-gated) anchors carry no href and are
  -- skipped by the attribute query. ids stay site-relative paths.
  local out = {}
  for _, href in ipairs(host.html.attr(body, "#chapterlist a", "href")) do
    local id = tostring(href):match("violetmanga%.com(/[^%?#]*)") or href
    local num = tostring(href):match("-chapter%-(.-)/?$") or ""
    out[#out + 1] = {
      id = id,
      number = tonumber(num) or 0,
      title = num ~= "" and ("Chapter " .. num) or "",
      release_at = 0, -- list carries no per-chapter dates
      pages = 0,
    }
  end
  return { ok = true, data = out }
end

-- ── pages ──────────────────────────────────────────────────────────────────

methods.pages = function(params)
  local id = params.id or ""
  local url = tostring(id)
  if not url:match("^https?://") then url = BASE .. id end
  local body, err = http_req(url)
  if err ~= nil then return { ok = false, error = err } end

  -- ts_reader.run({..., "sources": [{..., "images": [...]}]}): scan inline
  -- scripts for the "images" array; slashes may be JSON-escaped (\/).
  local images = body:match('"images"%s*:%s*%[(.-)%]')
  if not images then
    host.log.error("violetmanga: no images array in " .. url)
    return { ok = false, error = "violetmanga: no pages for " .. tostring(id) }
  end
  local out = {}
  for raw in images:gmatch("https?:\\?/\\?/[^\"']+") do
    out[#out + 1] = (raw:gsub("\\/", "/"))
  end
  if #out == 0 then
    host.log.error("violetmanga: no page images in " .. url)
    return { ok = false, error = "violetmanga: no pages for " .. tostring(id) }
  end
  return { ok = true, data = out }
end

-- ── dispatch ───────────────────────────────────────────────────────────────

function handle(req)
  local fn = methods[req.method]
  if fn == nil then return { ok = false, error = "unknown method " .. tostring(req.method) } end
  return fn(req.params or {})
end
