-- VioletManga plugin (violetmanga.com): WordPress mangareader/bixbox theme.
-- Port of goIsekai examples/plugins/lua/violetmanga. The source read page lists
-- via host.browser.evaluate (no cppIsekai equivalent); instead the ts_reader
-- SSR "images" array is regex-scanned from the reader HTML — the path the
-- source itself uses against the raw body. Envelope { ok, data | error }.
local BASE = "https://violetmanga.com"

-- ── helpers ────────────────────────────────────────────────────────────────

local http_req = host.http.fetch_body -- (body, err); nil unless 200 (plugin-host-promotion).

-- first text node of a selector; "" when absent

-- ── search ─────────────────────────────────────────────────────────────────

-- ── detail ─────────────────────────────────────────────────────────────────

-- ── chapters ───────────────────────────────────────────────────────────────

-- ── pages ──────────────────────────────────────────────────────────────────

-- ── dispatch ───────────────────────────────────────────────────────────────

local function sel1(html, css)
  local r = host.html.select(html, css)
  return r and r[1] or ""
end

return { BASE = BASE, http_req = http_req, sel1 = sel1 }
