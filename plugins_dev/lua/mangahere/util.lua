-- util.lua — MangaHere HTML/JS parsing helpers.
-- Port of goIsekai examples/plugins/lua/mangahere/util.lua; host.re2.* replaces
-- host.regex.* (pattern first, row[1] = full match), canonical field names,
-- chapter dates via a MONTHS table + civil-days epoch (host.text.date_to_iso is
-- not trusted with the "Nov 18,2016" layout), pages emitted as bare URL strings.
local util = {}

local BASE = "https://www.mangahere.cc"

-- first capture group of the first match; nil when no match (host re2.capture).
local function find1(s, pattern)
  return host.re2.capture(pattern, tostring(s or ""))
end

-- ─── Search result parsing ─────────────────────────────────────────────────
-- GET /search?title={q}&page={N} — server-rendered rows, two layouts:
--   <a href="/manga/{slug}/" title="T"><img class="manga-list-4-cover" src="COVER"
--   <p class="manga-list-4-item-title"><a href="/manga/{slug}/" title="T">T</a>

function util.parse_search(html)
  local results, seen = {}, {}

  local function add(slug, title, cover)
    if not slug or slug == "" or seen[slug] then return end
    seen[slug] = true
    results[#results + 1] = {
      id = slug,
      name = host.text.trim(host.text.unescape(title or "")),
      cover = host.text.unescape(cover or ""),
    }
  end

  for _, r in ipairs(host.re2.find_all(
    [[<a href="(/manga/[^"/]+/)" title="([^"]+)"><img class="manga-list-4-cover" src="([^"]+)"]], html)) do
    add(find1(r[2], [[^/manga/([^/]+)/]]), r[3], r[4])
  end
  for _, r in ipairs(host.re2.find_all(
    [[<p class="manga-list-4-item-title"><a href="(/manga/[^"/]+/)" title="([^"]+)"]], html)) do
    add(find1(r[2], [[^/manga/([^/]+)/]]), r[3], nil)
  end

  return results
end

-- ─── Manga detail parsing ──────────────────────────────────────────────────
-- GET /manga/{slug}/:
--   <span class="detail-info-right-title-font">TITLE</span>
--   <span class="detail-info-right-title-tip">Ongoing</span>
--   <p class="detail-info-right-say">Author: <a ...>NAME</a></p>
--   <p class="detail-info-right-tag-list"><a ...>Genre</a>...</p>
--   <p style="display:none" class="fullcontent">FULL SUMMARY</p>
--   <img class="detail-info-cover-img" src="COVER"

function util.parse_manga_detail(html, manga_id)
  local genres = {}
  local genre_block = find1(html, [[(?s)detail-info-right-tag-list">(.*?)</p>]]) or ""
  for _, r in ipairs(host.re2.find_all([[title="[^"]*">(.*?)</a>]], genre_block)) do
    local name = host.text.strip_html(r[2] or "")
    if name ~= "" then genres[#genres + 1] = name end
  end

  local author = host.text.strip_html(find1(html, [[detail-info-right-say">Author:\s*<a[^>]*>(.*?)</a>]]) or "")

  return {
    id = manga_id,
    name = host.text.trim(host.text.unescape(find1(html, [[detail-info-right-title-font">([^<]+)<]]) or "")),
    cover = host.text.unescape(find1(html, [[detail-info-cover-img" src="([^"]+)"]]) or ""),
    status = host.text.normalize_status(find1(html, [[detail-info-right-title-tip">([^<]+)<]]) or ""),
    tags = genres,
    release_at = 0, -- the detail page carries no machine-readable series date
    description = host.text.strip_html(find1(html, [[(?s)class="fullcontent">(.*?)</p>]]) or ""),
    authors = author ~= "" and { author } or {},
    url = BASE .. "/manga/" .. manga_id .. "/",
  }
end

-- ─── Chapter list parsing ──────────────────────────────────────────────────
-- Each row links /manga/{slug}/{cXX}/1.html followed by
-- <p class="title2">Nov 18,2016 </p>. Path and date are captured together so
-- every date stays pinned to its row; the number is rebuilt from the path
-- (padded/decimal safe).

-- "Nov 18,2016" etc -> unix seconds via host.text.date_epoch (D7, strptime
-- space-in-format matches the missing padding); 0 when unparseable.
local function date_epoch(s)
  return host.text.date_epoch(tostring(s or "")) or 0
end

function util.parse_chapter_list(html, manga_id)
  local chapters, seen = {}, {}

  -- capture only the chapter slug (c021.5), not the /1.html tail, so the num
  -- extraction anchors at the end of the string
  local pattern =
    [[(?s)<a href="/manga/]] .. manga_id .. [[/(c[0-9.]+)/1\.html"[^>]*>.*?<p class="title2">([^<]+)</p>]]
  for _, r in ipairs(host.re2.find_all(pattern, html)) do
    local path, raw_date = r[2], r[3]
    local num = tonumber(path:match("c([0-9][0-9.]*)$"))
    if num and not seen[path] then
      seen[path] = true
      chapters[#chapters + 1] = {
        id = manga_id .. ":" .. path,
        number = num,
        title = string.format("Chapter %g", num),
        release_at = date_epoch(host.text.trim(raw_date or "")),
        pages = 0, -- unknown until the chapterfun walk
      }
    end
  end

  -- ascending for the reader (the source shipped newest-first)
  table.sort(chapters, function(a, b) return a.number < b.number end)
  return chapters
end

-- ─── Dean-Edwards unpacker for the chapterfun p,a,c,k,e,d payload ──────────
-- Full packer tail: }('PAYLOAD',BASE,COUNT,'w|w'.split('|'),0,{})

local function unpack_packer(js)
  local m = host.re2.find([[(?s)\}\('(.*)',(\d+),\d+,'(.*)'\.split\('\|'\),0,\{\}\)]], js)
  if not m or #m < 4 then return nil end
  local payload, base, words = m[2], m[3], m[4]
  local n = tonumber(base)
  local list = {}
  local start = 1
  while true do
    local pipe = words:find("|", start, true)
    if not pipe then
      list[#list + 1] = words:sub(start)
      break
    end
    list[#list + 1] = words:sub(start, pipe - 1)
    start = pipe + 1
  end
  local function enc(x) -- base-n index -> packer key ('a'..'z','A'..'Z','0'..'9')
    local digits = ""
    while true do
      local r = x % n
      local c
      if r < 10 then
        c = tostring(r)
      elseif r < 36 then
        c = string.char(87 + r)
      else
        c = string.char(29 + r)
      end
      digits = c .. digits
      x = math.floor(x / n)
      if x == 0 then break end
    end
    return digits
  end
  local dict = {}
  for i, w in ipairs(list) do dict[enc(i - 1)] = w end
  return payload:gsub("%w+", dict)
end

-- ─── Page list via chapterfun (2 images per request) ───────────────────────
-- The DESKTOP reader page /manga/{id}/1.html carries var chapterid={cid} and
-- var imagecount={total}. chapterfun.ashx accepts an EMPTY key (unlike the
-- browser, which derives dm5_key from a packed eval). Each call returns a
-- packed JS with pix (folder) + 2 filenames; replies overlap [K,K+1]; dedupe
-- by filename. The packed pix truncates the folder decimal ("007." serves
-- "007.5") and KEEPS the "/compressed" tail, so the suffix is inserted INTO
-- the folder, not appended.

local function chapter_num_suffix(chapter_id)
  local num = find1(chapter_id, [[c([0-9][0-9.]*)$]]) or ""
  local frac = find1(num, [[\.([0-9]+)$]])
  if frac then return frac end
  return "0"
end

-- get_body(url, opts) is injected by main.lua (200-only body or nil); the
-- chapterfun requests carry the chapter Referer.
function util.get_page_urls(get_body, chapter_id)
  local base = BASE .. "/manga/" .. chapter_id .. "/"
  local first = get_body(base .. "1.html")
  if not first or first == "" then return nil end
  local cid = find1(first, [[var chapterid\s*=\s*([0-9]+);]])
  local total = tonumber(find1(first, [[var imagecount\s*=\s*(\d+);]]))
  if not cid then return nil end
  if not total or total < 1 then total = 1000 end

  local suffix = chapter_num_suffix(chapter_id)
  local seen, urls = {}, {}
  local k = 1
  while #urls < total and k <= total do
    local packed = get_body(
      BASE .. "/manga/" .. chapter_id .. "/chapterfun.ashx?cid=" .. cid .. "&page=" .. tostring(k) .. "&key=",
      { headers = { Referer = base .. tostring(math.ceil(k / 2)) .. ".html" } })
    if not packed or packed == "" then break end
    local js = unpack_packer(packed)
    if not js then break end
    -- re-insert the decimal suffix into the folder ("007." -> "007.5/")
    local pix = find1(js, [[pix="([^"]+)"]]) or ""
    pix = (pix:gsub("([0-9])%./", "%1." .. suffix .. "/", 1))
    for _, r in ipairs(host.re2.find_all([["(/[^"]+\.jpg)"]], js)) do
      local fname = r[2]
      if fname and not seen[fname] then
        seen[fname] = true
        urls[#urls + 1] = "https:" .. pix .. fname -- bare URL; per-image Referer is a host-side concern
      end
    end
    k = k + 2
  end
  return urls
end

return util
