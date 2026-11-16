-- util.lua — KaliScan HTML parsing helpers.
-- Port of goIsekai examples/plugins/lua/kaliscan/util.lua; host.re2.* replaces
-- host.regex.* (arg order pattern,subject; row[1] = full match, row[2..] groups).
-- Emits canonical fields (name/cover/tags) per the cppIsekai contracts.
local util = {}

-- first capture group of the first match; nil when the pattern does not match.
function util.find1(s, pattern)
  local m = host.re2.find(pattern, tostring(s or ""))
  if #m >= 2 then return m[2] end
  if #m == 1 then return m[1] end
  return nil
end

-- ─── Search result parsing ─────────────────────────────────────────────────
-- One result shape for both sources (same anchor):
--   * /service/backend/search/?q=… fragment — novel__item cards, cover in a
--     plain <img src="COVER">
--   * /genres/<slug> archive pages — book-item cards with a lazy placeholder
--     <img src='…x.gif' data-src="COVER">
-- The first double-quoted src= IS the real cover: the lazy card's placeholder
-- uses single quotes, the fragment card's src carries the URL directly.
function util.parse_search(html)
  local results, seen = {}, {}
  local rows = host.re2.find_all(
    [[(?s)<a\s+title="([^"]*?)"[^>]*href="/manga/([^"]*?)"[^>]*>\s*<img[^>]*?src="([^"]*?)"]], html)
  for _, r in ipairs(rows) do
    local title, slug, cover = r[2], r[3], r[4]
    if slug ~= "" and not seen[slug] then
      seen[slug] = true
      results[#results + 1] = { id = slug, name = title or "", cover = cover or "" }
    end
  end
  return results
end

-- ─── Manga detail parsing (/manga/SLUG) ────────────────────────────────────
--   <h1>TITLE</h1>                                  in <div class="name box">
--   Authors :</strong> <a><span>AUTHOR</span>       in the meta box
--   Status :</strong> <a><span>STATUS</span>
--   Cover: <div class="...cover..."> <img data-src="URL">
--   Description: <p class="content">TEXT</p>        in the summary tab
function util.parse_manga_detail(html, manga_id, base)
  local author = util.find1(html, [[Authors[^<]*</strong>\s*\n?\s*<a[^>]*>\s*<span>([^<]+)</span>]]) or ""
  local tags = {}
  for _, r in ipairs(host.re2.find_all([[href="/genres/([^/"]+)/"[^>]*>\s*([^<]+)\s*]], html)) do
    local name = r[3]:gsub("[%s,]+$", "")
    if name ~= "" then tags[#tags + 1] = name end
  end
  return {
    id = manga_id,
    name = util.find1(html, [[<h1>([^<]+)</h1>]]) or "",
    cover = util.find1(html, [[(?s)class="[^"]*cover[^"]*"[^>]*>.*?<img[^>]*data-src="([^"]*?)"]]) or "",
    status = host.text.normalize_status(
      util.find1(html, [[Status[^<]*</strong>\s*\n?\s*<a[^>]*>\s*<span>([^<]+)</span>]]) or ""),
    tags = tags,
    release_at = 0, -- the page carries no machine dates
    description = host.text.strip_html(util.find1(html, [[(?s)<p class="content"[^>]*>(.*?)</p>]]) or ""),
    authors = author ~= "" and { host.text.strip_html(author) } or {},
    url = (base or "") .. "/manga/" .. tostring(manga_id),
  }
end

-- ─── Chapter list parsing (/manga/SLUG) ────────────────────────────────────
--   <li id="c-NUMBER"><a href="/manga/SLUG/chapter-NUMBER" title="...">
--     <div><strong class="chapter-title">TITLE</strong>
--     <time class="chapter-update">UPLOAD_TIME</time>
-- chapter id: path after /manga/ with "/" swapped for ":" (host treats the id
-- as one opaque segment). Site HTML is newest-first already; no reversal.
function util.parse_chapter_list(html, manga_id)
  local chapters = {}
  local pat = 'id="c-([^"]*?)"[^>]*>\\s*'
    .. '<a[^>]*href="([^"]*?)"[^>]*title="([^"]*?)"[^>]*>\\s*'
    .. '<div>\\s*<strong class="chapter-title">([^<]*?)</strong>\\s*'
    .. "<time[^>]*>([^<]*)</time>"
  for _, r in ipairs(host.re2.find_all(pat, html)) do
    local number, href, title = r[2], r[3], r[4]
    local chapter_id = util.find1(href, [[(?s)/manga/(.+)]]) or (manga_id .. "/chapter-" .. number)
    chapters[#chapters + 1] = {
      id = chapter_id:gsub("/", ":"),
      number = tonumber(number) or 0,
      title = title or "",
      release_at = 0, -- relative phrases ("2 days ago"); contract wants epoch ints
      pages = 0,
    }
  end
  return chapters
end

-- ─── Page list parsing (chapterServer response) ────────────────────────────
-- Each page image: <div ... data-src="URL" ...>; svg placeholders and /static/
-- assets are not pages.
function util.parse_page_list(html)
  local pages = {}
  for _, r in ipairs(host.re2.find_all([[data-src="([^"]*?)"]], html)) do
    local url = r[2]
    if #host.re2.find([[\.svg$]], url) == 0 and #host.re2.find([[/static/]], url) == 0 then
      pages[#pages + 1] = url
    end
  end
  return pages
end

return util
