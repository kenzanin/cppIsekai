-- util.lua — FanFox (m.fanfox.net / fanfox.net, DM5 stack) parsers.
-- Port of goIsekai examples/plugins/lua/fanfox/util.lua to cppIsekai:
-- host.re2.* (pattern-first, row tables), json globals, and fetch in-module
-- (cppIsekai has no get_body to pass in, so util.http_body replaces the
-- get_body parameter of get_page_urls).
local util = {}

-- host.text.trim: utf8-aware, case-preserving (plugin-host-promotion).
local trim = host.text.trim

-- re2.find row capture, nil-safe for `or` chains ({} on no-match).
local function cap(pat, s, n)
  return host.re2.find(pat, tostring(s or ""))[n or 2]
end

-- GET + 200 -> body, else nil. opts pass straight to host.http.fetch
-- (headers = {...} for the chapterfun Referer).
-- GET + 200 -> body, else nil (empty body included). opts pass straight to
-- host.http.fetch (headers = {...} for the chapterfun Referer).
function util.http_body(url, opts)
  local b = host.http.fetch_body(url, opts)
  if b == nil or b == "" then
    return nil
  end
  return b
end

-- ─── Search result parsing ─────────────────────────────────────────────────
-- GET https://fanfox.net/search?title=...&page=N (desktop site). Each result
-- renders twice (cover row then title row); both anchors carry title="...".
-- Bare numeric links (/manga/10891) are redirects and are skipped.
function util.parse_search(html)
  local results, seen = {}, {}

  local function add(slug, title, cover)
    -- tonumber() rejects numeric redirect slugs like "10891".
    if not slug or slug == "" or tonumber(slug) ~= nil then
      return
    end
    if seen[slug] then
      return
    end
    seen[slug] = true
    results[#results + 1] = {
      id = slug,
      name = trim(host.text.unescape(title or "")),
      cover = host.text.unescape(cover or ""),
    }
  end

  for _, m in ipairs(host.re2.find_all([[href="(/manga/[^"/]+/)" title="([^"]+)">\s*<img[^>]*src="([^"]+)"]], html)) do
    add(cap([[^/manga/([^/]+)/]], m[2]), m[3], m[4])
  end
  for _, m in ipairs(host.re2.find_all([[href="(/manga/[^"/]+/)" title="([^"]+)"]], html)) do
    add(cap([[^/manga/([^/]+)/]], m[2]), m[3], "")
  end

  return results
end

-- ─── Manga detail parsing ─────────────────────────────────────────────────
-- GET https://m.fanfox.net/manga/{slug}/ (mobile detail page). The first
-- <div class="title"> is the manga name; a later widget with the same class
-- is a login prompt, so first-match-wins is load-bearing.
function util.parse_manga_detail(html, manga_id)
  local name = trim(host.text.unescape(cap([[class="title">([^<]+)<]], html) or ""))

  -- Real markup: ... <img src="//fmcdn..." ... class="detail-cover" /> — the
  -- class comes AFTER src; match the detail-cover img itself, then normalize
  -- the protocol-relative URL.
  local cover = cap([[<img src="([^"]+)"[^>]*class="detail-cover"]], html) or ""
  cover = host.text.unescape(cover)
  if cover:sub(1, 2) == "//" then
    cover = "https:" .. cover
  end

  -- Author(s) share a <p> with Artist(s): cut at <br or end of paragraph.
  local author = host.text.strip_html(cap([[Author\(s\):\s*(.-)(?:<br|</p>)]], html) or "")

  local status = host.text.normalize_status(cap([[Status:\s*([^<]+)<]], html) or "")

  local description = host.text.strip_html(cap([[(?s)<div class="manga-summary">(.*?)</div>]], html) or "")

  local genres = {}
  local genre_block = cap([[(?s)<div class="manga-genres">(.*?)</div>]], html) or ""
  for _, m in ipairs(host.re2.find_all([[<a[^>]*>(.*?)</a>]], genre_block)) do
    local fname = host.text.strip_html(m[2])
    if fname ~= "" then
      genres[#genres + 1] = fname
    end
  end

  return {
    id = manga_id,
    name = name,
    cover = cover,
    status = status,
    tags = genres,
    release_at = 0, -- detail page renders no machine date
    description = description,
    authors = author ~= "" and { author } or {},
    artists = {},
    url = "https://m.fanfox.net/manga/" .. manga_id .. "/",
  }
end

-- ─── Chapter list parsing ─────────────────────────────────────────────────
-- Links: href="//m.fanfox.net/manga/{slug}/[vNN/]cNNN/1.html". Chapter id is
-- the path after /manga/ minus /1.html ("blue_lock/c161"). Decimal chapters
-- (c288.5) are first-class: number regex c([0-9][0-9.]*)$ anchored to the
-- path end. Row text is "Ch N" + nbsp padding + date; title rebuilt from num.
function util.parse_chapter_list(html, manga_id)
  local chapters, seen = {}, {}
  -- scope to this manga's links so other series' /1.html never leak in
  local pattern = [[href="//m\.fanfox\.net/manga/]] .. manga_id .. [[/([^"]+)/1\.html"]]

  for _, m in ipairs(host.re2.find_all(pattern, html)) do
    local rest = m[2]
    local num = tonumber(cap([[c([0-9][0-9.]*)$]], rest))
    if num ~= nil and not seen[rest] then
      seen[rest] = true
      chapters[#chapters + 1] = {
        id = manga_id .. "/" .. rest,
        number = num,
        title = string.format("Chapter %g", num),
        release_at = 0, -- chapter rows carry no machine date
        pages = 0,
      }
    end
  end

  -- newest-first house order regardless of DOM order
  table.sort(chapters, function(a, b)
    return a.number > b.number
  end)
  return chapters
end

-- ─── Desktop chapterfun path (batched, 2 images per request) ──────────────
-- The mobile path costs one request PER page. The desktop reader exposes
-- var chapterid / var imagecount; chapterfun.ashx returns Dean-Edwards packed
-- JS with pix plus 2 token-stamped filenames per call; replies overlap
-- ([K,K+1]), so dedupe by filename. An empty key works (curl-verified).

-- Dean-Edwards unpacker for the p,a,c,k,e,d payload.
local function unpack_packer(js)
  -- }('PAYLOAD',BASE,COUNT,'w|w'.split('|'),0,{})
  local m = host.re2.find([[(?s)\}\('(.*)',(\d+),\d+,'(.*)'\.split\('\|'\),0,\{\}\)]], js)
  local payload, base, words = m[2], m[3], m[4]
  if payload == nil then
    return nil
  end
  local n = tonumber(base)
  local list, start = {}, 1
  while true do
    local pipe = words:find("|", start, true)
    if pipe == nil then
      list[#list + 1] = words:sub(start)
      break
    end
    list[#list + 1] = words:sub(start, pipe - 1)
    start = pipe + 1
  end
  local function enc(x)
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
      if x == 0 then
        break
      end
    end
    return digits
  end
  local dict = {}
  for i, w in ipairs(list) do
    dict[enc(i - 1)] = w
  end
  return payload:gsub("%w+", dict)
end

local function chapter_num_suffix(chapter_id)
  -- "slug/c008" -> "0"; "slug/v02/c008.5" -> "5"
  local num = cap([[c([0-9][0-9.]*)$]], chapter_id) or ""
  if cap([[\.([0-9]+)$]], num) then
    return cap([[([0-9]+)$]], num)
  end
  return "0"
end

-- Resolves the reader cid and walks chapterfun.ashx in pairs; nil when the
-- chapter page or unpacker fails. Returns plain URL strings (house pages
-- contract); the per-image Referer header is folded into the chapterfun
-- request and the host image-proxy covers the image fetch.
function util.get_page_urls(chapter_id)
  local base = "https://fanfox.net/manga/" .. chapter_id .. "/"
  local first = util.http_body(base .. "1.html")
  if not first then
    return nil
  end

  local cid = cap([[var chapterid\s*=\s*([0-9]+);]], first)
  local total = tonumber(cap([[var imagecount\s*=\s*(\d+);]], first))
  if cid == nil then
    return nil
  end
  if total == nil or total < 1 then
    total = 1000
  end

  local suffix = chapter_num_suffix(chapter_id)
  local seen, urls, k = {}, {}, 1
  while #urls < total and k <= total do
    local packed = util.http_body(
      "https://fanfox.net/chapterfun.ashx?cid=" .. cid .. "&page=" .. tostring(k) .. "&key=",
      { headers = { Referer = base .. tostring(math.ceil(k / 2)) .. ".html" } }
    )
    if not packed then
      break
    end
    local js = unpack_packer(packed)
    if js == nil then
      break
    end
    -- re-insert the truncated decimal into the folder ("02-008." ->
    -- "02-008.5"); the /compressed tail stays after it
    local pix = (cap([[pix="([^"]+)"]], js) or ""):gsub("([0-9])%./", "%1." .. suffix .. "/", 1)
    for _, fm in ipairs(host.re2.find_all([["(/[^"]+\.jpg[^"]*)"]], js)) do
      local fname = fm[2]
      if not seen[fname] then
        seen[fname] = true
        urls[#urls + 1] = "https:" .. pix .. fname
      end
    end
    k = k + 2
  end
  return urls
end

return util
