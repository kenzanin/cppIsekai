-- Comix.to plugin (https://comix.to): Nuxt SSR + SPA reader.
-- Port of goIsekai examples/plugins/lua/comix. cppIsekai has no browser.* API:
-- every page goes through the lightpanda headless-render profile instead, so
-- the site's JS still runs (the signed-API/SPA constraints from the source
-- remain). What cannot be replicated headless: the reader's localStorage
-- preload flag and per-page scroll nudging -- pages harvest whichever
-- img[src] the render wait materialized.
-- Envelope { ok, data | error }; handle(req) entry.
local BASE = host.plugin.param("site_url") or "https://comix.to"

local RENDER = { profile = "lightpanda", timeout_ms = 60000 }

-- ── helpers ────────────────────────────────────────────────────────────────

local function render(url)
  local res, err = host.http.fetch(url, RENDER)
  if err ~= nil then return nil, err end
  if res.status ~= 200 then return nil, "comix: upstream status " .. tostring(res.status) end
  return res.body, nil
end

-- first capture group; nil when the pattern does not match.
local function find1(s, pattern)
  local m = host.re2.find(pattern, tostring(s or ""))
  if #m >= 2 then return m[2] end
  if #m == 1 then return m[1] end
  return nil
end

-- cover_abs absolutizes bare /images/covers/... paths (both the SSR detail
-- JSON and the browse cards emit them); a relative cover would resolve
-- against the host instead of comix.
local function cover_abs(url)
  if type(url) ~= "string" or url == "" then return "" end
  if url:sub(1, 1) == "/" then return BASE .. url end
  return url
end

-- The detail/genre payload is embedded in script#initial-data (JSON).
local function initial_data(html)
  return find1(html or "", [[(?s)<script type="application/json" id="initial-data">(.*?)</script>]])
end

-- comix renders relative chapter dates ("2mos ago"); host.text.date_to_iso
-- parses the long forms, so short aliases normalize first (unit map mirrors
-- the reference comix extension). Returns "" when unparseable -> host zero.
local ALIAS = {
    m = "minute", min = "minute", mins = "minute",
    h = "hour", hr = "hour", hrs = "hour",
    d = "day", day = "day", days = "day",
    w = "week", week = "week", weeks = "week",
    mo = "month", mos = "month", month = "month", months = "month",
    y = "year", yr = "year", yrs = "year", year = "year", years = "year",
}

local function relative_to_iso(s)
  s = host.text.trim(s or "")
  if s == "" then return "" end
  local iso = host.text.date_to_iso(s)
  if iso then return iso end
  local n, unit = string.match(string.lower(s), "^%s*(%d+)%s*([%a]+)%s+ago%s*$")
  if not n then return "" end
  local long = ALIAS[unit]
  if not long then return "" end
  return host.text.date_to_iso(n .. " " .. long .. " ago") or ""
end

local methods = {}

-- ── search ─────────────────────────────────────────────────────────────────

methods.search = function(params)
  local query = params.title or params.query or params.q or ""
  if query == "" then return { ok = true, data = { items = {}, total = 0 } } end
  -- The browse paginates (~30/page); two pages like the source -- enough for
  -- the grid, bounded call budget. Dedupe by slug across pages.
  local out, seen = {}, {}
  local base = BASE .. "/browse?q=" .. host.url_encode(query)
  for page = 1, 2 do
    local body, err = render(base .. "&page=" .. page)
    if err ~= nil or not body or body == "" then break end
    local added = 0
    local pat = [[(?s)<a class="lrow__poster" href="/title/([^"]+)" aria-label="([^"]+)">.*?src="([^"]+)"]]
    for _, m in ipairs(host.re2.find_all(pat, body)) do
      local slug, title, cover = m[2], m[3], m[4]
      if not seen[slug] then
        seen[slug] = true
        -- aria-label is HTML-escaped; decode entities.
        out[#out + 1] = { id = slug, name = host.text.unescape(title or ""), cover = cover_abs(cover) }
        added = added + 1
      end
    end
    if added == 0 then break end
  end
  host.log.info("comix: search found " .. tostring(#out) .. " for " .. query)
  return { ok = true, data = { items = out, total = #out } }
end

-- ── detail ─────────────────────────────────────────────────────────────────

methods.detail = function(params)
  local slug = params.id or ""
  local empty = { id = slug, name = "", cover = "", status = "unknown", tags = {}, release_at = 0,
    authors = {}, artists = {}, description = "", url = BASE .. "/title/" .. slug }
  if slug == "" then return { ok = true, data = empty } end

  local body, err = render(BASE .. "/title/" .. host.url_encode(slug))
  if err ~= nil then return { ok = false, error = err } end

  local raw = initial_data(body)
  if not raw then
    host.log.error("comix: no initial-data in detail page: " .. slug)
    return { ok = false, error = "envelope_unrecognised: no initial-data" }
  end
  local ok, data = pcall(json.decode, raw)
  if not ok or type(data) ~= "table" or type(data.queries) ~= "table" then
    return { ok = false, error = "envelope_unrecognised" }
  end
  -- The detail query is keyed ["manga","detail","<hid>"].
  for k, v in pairs(data.queries) do
    if type(k) == "string" and string.find(k, '"detail"', 1, true) and type(v) == "table" then
      local genres = {}
      if type(v.genres) == "table" then
        for _, g in ipairs(v.genres) do
          if type(g) == "table" then
            local name = g.title or g.slug or g.name
            if type(name) == "string" then genres[#genres + 1] = name end
          elseif type(g) == "string" then
            genres[#genres + 1] = g
          end
        end
      end
      -- poster is {medium=..., large=...}; take the largest. Anything
      -- non-string stays "" so cover is always a string.
      local cover = ""
      if type(v.cover) == "string" then
        cover = v.cover
      elseif type(v.poster) == "string" then
        cover = v.poster
      elseif type(v.poster) == "table" then
        if type(v.poster.large) == "string" then cover = v.poster.large
        elseif type(v.poster.medium) == "string" then cover = v.poster.medium end
      end
      local d = {
        id = slug,
        name = v.title or "",
        cover = cover_abs(cover),
        status = host.text.normalize_status(type(v.status) == "string" and v.status or ""),
        tags = genres,
        release_at = 0,
        authors = (type(v.author) == "string" and v.author ~= "") and { v.author } or {},
        artists = (type(v.artist) == "string" and v.artist ~= "") and { v.artist } or {},
        description = v.synopsis or v.description or "",
        url = BASE .. "/title/" .. slug,
      }
      return { ok = true, data = d }
    end
  end
  host.log.error("comix: no detail query in initial-data: " .. slug)
  return { ok = false, error = "envelope_unrecognised: no detail query" }
end

-- ── chapters ───────────────────────────────────────────────────────────────

-- Chapter rows are client-rendered AND client-paginated: the title page
-- carries 20 rows plus a .npager ("Showing 1 to 20 of N", num buttons) —
-- no per-page URL, the pager is buttons. So the harvest runs in the page:
-- grab rows, click the next .npager__num, wait for the row set to change,
-- repeat. Dedupe by href across pages; cap 40 pages / 30s.
local CH_HARVEST = '(function(){return new Promise(function(res){var t0=Date.now();var all={},order=[]; function rows(){return Array.prototype.slice.call(document.querySelectorAll("a.mchap-row__primary")).map(function(a){var r=a.closest(".mchap-row")||a.parentElement;var c=r&&r.querySelector(".mchap-row__ch");var t=r&&r.querySelector(".mchap-row__time");return{h:a.getAttribute("href")||"",c:c?c.textContent.trim():"",t:t?t.textContent.trim():""};});} function absorb(){rows().forEach(function(r){if(r.h&&!(r.h in all)){all[r.h]=1;order.push(r);}});} function btn(n){var bs=document.querySelectorAll(".npager__num");for(var i=0;i<bs.length;i++){if(bs[i].textContent.trim()===String(n))return bs[i];}return null;} function hint(){var h=document.querySelector(".mchap-foot__hint");return h?h.textContent.replace(/\\s+/g," "):"";} function perPage(){var m=hint().match(/to\\s+(\\d+)\\s+of/);return m?parseInt(m[1],10):0;} function total(){var m=hint().match(/of\\s+(\\d+)/);return m?parseInt(m[1],10):0;} function first(){var a=document.querySelector("a.mchap-row__primary");return a?a.getAttribute("href"):"";} function done(){res(JSON.stringify(order));} function page(n){absorb();var tot=total();if((tot&&order.length>=tot)||n>40||Date.now()-t0>45000)return done(); var b=btn(n);if(!b)return done(); var pp=perPage();var want=pp?String((n-1)*pp+1):null; var before=hint()+"|"+first();var start=Date.now();var tries=0; (function w(){var h=hint(); var ok=want?(h.indexOf(want+" to ")>=0):(h+"|"+first()!==before); if(ok){setTimeout(function(){absorb();page(n+1);},150);} else if(Date.now()-start>6000){ if(tries<1){tries++;var nb=btn(n);if(nb)nb.click();start=Date.now();setTimeout(w,200);} else{absorb();page(n+1);} } else setTimeout(w,150);})();} (function boot(){if(document.querySelector("a.mchap-row__primary")){absorb();page(2);}else if(Date.now()-t0>15000){done();}else{setTimeout(boot,150);}})(); });})()'

methods.chapters = function(params)
  local slug = params.id or ""
  if slug == "" then return { ok = true, data = {} } end

  local _, gerr = host.browser.call("goto", { url = BASE .. "/title/" .. host.url_encode(slug), timeout_ms = 60000 })
  if gerr ~= nil then return { ok = false, error = "comix: goto failed: " .. gerr } end
  local out, eerr = host.browser.call("evaluate", { script = CH_HARVEST, timeout_ms = 60000 })
  if eerr ~= nil then return { ok = false, error = "comix: harvest failed: " .. eerr } end
  local ok, rows = pcall(json.decode, out or "")
  if not ok or type(rows) ~= "table" then
    return { ok = false, error = "comix: unexpected chapter harvest for " .. slug }
  end
  if #rows == 0 then
    -- upstream /api/v1 sits behind an on/off CF turnstile; one clean retry
    -- (fresh goto re-runs the page's own challenge path) before giving up
    local _, _ = host.browser.call("goto", { url = BASE .. "/title/" .. host.url_encode(slug) })
    out, eerr = host.browser.call("evaluate", { script = CH_HARVEST, timeout_ms = 60000 })
    if eerr ~= nil then return { ok = false, error = "comix: harvest failed: " .. eerr } end
    ok, rows = pcall(json.decode, out or "")
    if not ok or type(rows) ~= "table" then
      return { ok = false, error = "comix: unexpected chapter harvest for " .. slug }
    end
  end

  local out = {}
  for _, r in ipairs(rows) do
    local href, chtxt, when = r.h or "", r.c or "", r.t or ""
    local chid = string.match(href or "", "/([^/]+)$") or ""
    if chid ~= "" then
      -- Chapter number from "Ch.43.2" or the URL tail.
      local num = string.match(chtxt or "", "Ch%.([%d%.]+)") or string.match(chid, "chapter%-([%d%.]+)")
      local item = {
        id = slug .. ":" .. chid,
        number = tonumber(num) or 0,
        title = chtxt or "",
        release_at = 0,
        pages = 0,
      }
      local iso = relative_to_iso(when)
      if iso ~= "" then item.release_at = iso end
      out[#out + 1] = item
    end
  end
  if #out == 0 then
    host.log.error("comix: no chapter rows rendered for " .. slug)
    return { ok = false, error = "comix: no chapters for " .. slug }
  end
  host.log.info("comix: found " .. tostring(#out) .. " chapters for " .. slug)
  -- Site renders newest-first; keep document order.
  return { ok = true, data = out }
end

-- ── pages ──────────────────────────────────────────────────────────────────

-- The page list exists only in the rendered reader: the chapter API returns
-- an encrypted payload the site JS decrypts, and img srcs appear lazily via
-- IntersectionObserver. host.browser (change browser-host) runs the page's
-- own JS: goto, then ONE evaluate whose promise scrolls every .rpage-page
-- slot and collects the img src it materialized (lightpanda awaits promises;
-- verified: 6/6 urls, ~0.9s, image/webp without Referer).
local HARVEST = '(function(){return new Promise(function(res){var t0=Date.now();function urls(){return Array.prototype.slice.call(document.querySelectorAll(".rpage-page img")).map(function(im){return im.getAttribute("src");}).filter(Boolean);}function done(){res(JSON.stringify(urls()));}function scroll(slots,i){if(i<slots.length){slots[i].scrollIntoView();setTimeout(function(){scroll(slots,i+1);},150);}else{done();}}function wait(){var slots=document.querySelectorAll(".rpage-page");if(slots.length)return scroll(slots,0);if(Date.now()-t0>8000)return done();setTimeout(wait,100);}wait();});})()'

methods.pages = function(params)
  local composite = params.id or ""
  local slug, chid = string.match(composite, "^([^:]+):(.+)$")
  if not slug or not chid or slug == "" then
    return { ok = false, error = "comix: chapter id must be slug:chapterId, got " .. tostring(composite) }
  end
  local _, gerr = host.browser.call("goto", { url = BASE .. "/title/" .. host.url_encode(slug) .. "/" .. host.url_encode(chid), timeout_ms = 60000 })
  if gerr ~= nil then return { ok = false, error = "comix: goto failed: " .. gerr } end
  local out, eerr = host.browser.call("evaluate", { script = HARVEST, timeout_ms = 60000 })
  if eerr ~= nil then return { ok = false, error = "comix: harvest failed: " .. eerr } end
  local ok, urls = pcall(json.decode, out or "")
  if not ok or type(urls) ~= "table" then
    return { ok = false, error = "comix: unexpected harvest payload for " .. composite }
  end
  if #urls == 0 then
    host.log.error("comix: no page images in chapter: " .. composite)
    return { ok = false, error = "comix: no pages for " .. composite }
  end
  host.log.info("comix: found " .. tostring(#urls) .. " pages for " .. composite)
  return { ok = true, data = urls }
end

-- ── genres ─────────────────────────────────────────────────────────────────

methods.genres = function()
  local body, err = render(BASE .. "/browse")
  local raw = (err == nil and body) and initial_data(body) or nil
  if not raw then return { ok = true, data = {} } end
  local ok, data = pcall(json.decode, raw)
  if not ok or type(data) ~= "table" then return { ok = true, data = {} } end
  -- Genres live under list.options.genres, each {id, label, slug}. The walk
  -- accepts any depth; label+slug presence is the entry test.
    local found = {}
    local function walk(v)
      if type(v) ~= "table" then return end
      local label = v.label or v.name
      if type(label) == "string" and type(v.slug) == "string" and #label < 40 then
        found[#found + 1] = { name = label, slug = v.slug }
        return
      end
      for _, child in pairs(v) do
        walk(child)
      end
    end
    walk(data)
  local seen, genres = {}, {}
  for _, g in ipairs(found) do
    if not seen[g.slug] then
      seen[g.slug] = true
      genres[#genres + 1] = g
    end
  end
  host.log.info("comix: found " .. tostring(#genres) .. " genres")
  return { ok = true, data = genres }
end

function handle(req)
  local fn = methods[req.method]
  if fn == nil then return { ok = false, error = "unknown method " .. tostring(req.method) } end
  return fn(req.params or {})
end
