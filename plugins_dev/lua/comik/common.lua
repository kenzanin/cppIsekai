-- comik.to: translation-hub aggregator (manga grouped by scanlation source),
-- pure Qwik SSR. Port of goIsekai examples/plugins/lua/comik to the cppIsekai
-- host ABI (host.http.fetch + status check; host.re2.* pattern-first).
-- Pages are plain URL strings; chapter release_at is unix seconds (house
-- mangadex precedent) instead of goIsekai's RFC3339 string requirement.
local BASE = "https://comik.to"

-- host.text.trim: utf8-aware, case-preserving (plugin-host-promotion).
local trim = host.text.trim

-- http request -> (body, err): the host two-value fetch_body form.
local http_req = host.http.fetch_body

-- re2 find_all (pattern, body); nil on bad pattern (pcall) so callers can skip.
local function find_all(body, pattern)
  local ok, res = pcall(host.re2.find_all, pattern, tostring(body or ""))
  if not ok then return nil end
  return res
end

-- <time data-time="..."> epoch ms -> unix seconds; nil when absent.
local function epoch_ms_to_secs(ms)
  local n = tonumber(ms)
  if not n then return nil end
  return math.floor(n / 1000)
end


-- ── search ─────────────────────────────────────────────────────────────────
-- Cards: <a href="/title/<id>" : class="block relative"><img alt="<title>"
-- src="<cover>"...>; the anchor repeats with the alias list, so dedup by id.
-- The site paginates server-side with ?page=N (48 cards/page) but renders no
-- total in its Qwik state. Probe the last page exponentially, then binary-search
-- it, memoized per query -- same trick as the mangagoku port. Without this the
-- host got total = #items and every ?page=N looked identical.
local per_page = 48
local page_cache = {}

local function fetch_search(query, page)
  local url = BASE .. "/search?word=" .. host.url_encode(query)
  if page > 1 then url = url .. "&page=" .. page end
  local body, err = http_req(url)
  if not body then return nil, err end
  local items, seen = {}, {}
  for _, m in ipairs(find_all(body,
    [[(?s)<a href="/title/([a-z0-9]+)" : class="block relative"><img[^>]*alt="([^"]*)"[^>]*src="([^"]+)"]]
  ) or {}) do
    local id, title, cover = m[2], m[3], m[4]
    if id and not seen[id] then
      seen[id] = true
      items[#items + 1] = {
        id = id,
        name = host.text.unescape(trim(title)),
        cover = cover:find("^http") and cover or (BASE .. cover),
      }
    end
  end
  return items
end

local function page_info(query, first_count)
  local hit = page_cache[query]
  if hit then return hit end
  local good, step = 1, 1
  while good < 256 do
    local probe = math.min(good + step, 256)
    if #(fetch_search(query, probe) or {}) == 0 then break end
    good = probe
    step = step * 2
  end
  local lo, hi = good + 1, math.min(good + step, 256)
  while lo < hi do
    local mid = math.floor((lo + hi) / 2)
    if #(fetch_search(query, mid) or {}) == 0 then hi = mid else lo = mid + 1 end
  end
  local pages = lo - 1
  local last = pages == 1 and first_count or #(fetch_search(query, pages) or {})
  hit = { pages = pages, total = (pages - 1) * per_page + last }
  page_cache[query] = hit
  return hit
end



return { BASE=BASE, trim=trim, http_req=http_req, find_all=find_all, epoch_ms_to_secs=epoch_ms_to_secs, per_page=per_page, page_cache=page_cache, fetch_search=fetch_search, page_info=page_info }
