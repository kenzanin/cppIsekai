-- MangaHere (www.mangahere.cc): server-rendered search/detail; chapter pages
-- hide behind a Dean-Edwards packer served by chapterfun.ashx (2 images per
-- call, empty key ok). Port of goIsekai examples/plugins/lua/mangahere;
-- host.re2.* replaces host.regex.* (pattern first), envelope
-- { ok, data | error }, canonical fields per the mangadex plugin.
local BASE = "https://www.mangahere.cc" -- manifest site_url; no params, pinned like the other house ports

local util = require("util")
local genres = require("genres")

-- ids arrive as bare strings; tolerate {id=...} wrappers like the source did

-- request -> (body, err): the host two-value fetch_body form; opts (with
-- opts.headers) pass straight through.
local http_body = host.http.fetch_body


-- ── pager ─────────────────────────────────────────────────────────────────
-- The site's pager (div.pager-list) window-links /search?..page=N..; the real
-- result count is nowhere in the HTML, so the total is derived batcave-style:
-- (maxpage-1)*per_page + last_page_count, memoised per query so only the
-- first host page of a query pays the extra probe fetches.
local total_memo = {}



-- ── search ─────────────────────────────────────────────────────────────────
-- /search?title=Q&page=N; genre browse adds &genres=<numeric id> — the empty
-- title is valid and returns genre-only cards (HAR-confirmed upstream).


-- ── detail / chapters ──────────────────────────────────────────────────────
-- Both parse /manga/{id}/; chapter rows carry cXX slugs + "Nov 18,2016" dates.



-- ── pages ──────────────────────────────────────────────────────────────────
-- Chapter id "slug:c007.5" -> path "slug/c007.5"; util.get_page_urls resolves
-- cid from the reader page and walks chapterfun.ashx in pairs.


-- ── genres ─────────────────────────────────────────────────────────────────


local function id_of(params)
  local v = params.id
  if type(v) == "table" then v = v.id end
  return tostring(v or "")
end

local function pager_pages(body)
  local block = body:match('(<div class="pager%-list.-</div></div>)') or body
  local maxp = 1
  for n in block:gmatch("page=(%d+)") do
    local v = tonumber(n)
    if v and v > maxp then maxp = v end
  end
  return maxp
end

local function remember(key, value)
  if #total_memo > 128 then total_memo = {} end
  total_memo[key] = value
  return value
end

return { BASE=BASE, util=util, genres=genres, http_body=http_body, total_memo=total_memo, id_of=id_of, pager_pages=pager_pages, remember=remember }
