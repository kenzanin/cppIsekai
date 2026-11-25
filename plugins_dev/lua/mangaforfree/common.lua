-- MangaForFree.net: WordPress Madara theme, AdminAjax chapter mode.
-- Port of goIsekai extension-source/src/all/mangaforfree (MangaForFree.kt).
-- Search/detail are plain HTML; chapters come from admin-ajax.php
-- action=manga_get_chapters&manga=<post id>; reader images are plain src on
-- img.wp-manga-chapter-img. Raw chapters are skipped for the en catalogue.
local BASE = host.plugin.param("site_url") or "https://mangaforfree.net"
local AJAX = BASE .. "/wp-admin/admin-ajax.php"
local OPTS = {
  timeout_ms = 45000,
  headers = {
    ["User-Agent"] = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/158.0.0.0 Safari/537.36",
    ["Referer"] = BASE .. "/",
  },
}



methods = {}






local function http_req(url, extra)
  if not extra then return host.http.fetch_body(url, OPTS) end
  local o = {}
  for k, v in pairs(OPTS) do o[k] = v end
  for k, v in pairs(extra) do o[k] = v end
  return host.http.fetch_body(url, o)
end

local function find1(s, pat) return host.re2.capture(pat, tostring(s or "")) end
local function clean(s) return host.text.trim(host.text.unescape(host.text.strip_html(tostring(s or "")))) end
local function first(t) return type(t) == "table" and t[1] or nil end

local function manga_url(slug) return BASE .. "/manga/" .. host.url_encode(slug) .. "/" end

local function parse_cards(body)
  local hrefs = host.html.attr(body, "a[href*='/manga/'][title]", "href")
  local titles = host.html.attr(body, "a[href*='/manga/'][title]", "title")
  local covers = host.html.attr(body, "a[href*='/manga/'][title] img", "src")
  local out = {}
  for i, href in ipairs(hrefs) do
    local slug = find1(href, [[/manga/([^/]+)]])
    if slug and slug ~= "" and not slug:find("^%?") then
      out[#out + 1] = {
        id = slug,
        name = clean(titles[i] or ""),
        cover = covers[i] or "",
        url = href,
      }
    end
  end
  local seen, uniq = {}, {}
  for _, it in ipairs(out) do
    if not seen[it.id] then seen[it.id] = true; uniq[#uniq + 1] = it end
  end
  return uniq
end

local function clean(s) return host.text.trim(host.text.unescape(host.text.strip_html(tostring(s or "")))) end
local function first(t) return type(t) == "table" and t[1] or nil end

local function manga_url(slug) return BASE .. "/manga/" .. host.url_encode(slug) .. "/" end

local function parse_cards(body)
  local hrefs = host.html.attr(body, "a[href*='/manga/'][title]", "href")
  local titles = host.html.attr(body, "a[href*='/manga/'][title]", "title")
  local covers = host.html.attr(body, "a[href*='/manga/'][title] img", "src")
  local out = {}
  for i, href in ipairs(hrefs) do
    local slug = find1(href, [[/manga/([^/]+)]])
    if slug and slug ~= "" and not slug:find("^%?") then
      out[#out + 1] = {
        id = slug,
        name = clean(titles[i] or ""),
        cover = covers[i] or "",
        url = href,
      }
    end
  end
  local seen, uniq = {}, {}
  for _, it in ipairs(out) do
    if not seen[it.id] then seen[it.id] = true; uniq[#uniq + 1] = it end
  end
  return uniq
end

local function first(t) return type(t) == "table" and t[1] or nil end

local function manga_url(slug) return BASE .. "/manga/" .. host.url_encode(slug) .. "/" end

local function parse_cards(body)
  local hrefs = host.html.attr(body, "a[href*='/manga/'][title]", "href")
  local titles = host.html.attr(body, "a[href*='/manga/'][title]", "title")
  local covers = host.html.attr(body, "a[href*='/manga/'][title] img", "src")
  local out = {}
  for i, href in ipairs(hrefs) do
    local slug = find1(href, [[/manga/([^/]+)]])
    if slug and slug ~= "" and not slug:find("^%?") then
      out[#out + 1] = {
        id = slug,
        name = clean(titles[i] or ""),
        cover = covers[i] or "",
        url = href,
      }
    end
  end
  local seen, uniq = {}, {}
  for _, it in ipairs(out) do
    if not seen[it.id] then seen[it.id] = true; uniq[#uniq + 1] = it end
  end
  return uniq
end

local function manga_url(slug) return BASE .. "/manga/" .. host.url_encode(slug) .. "/" end

local function parse_cards(body)
  local hrefs = host.html.attr(body, "a[href*='/manga/'][title]", "href")
  local titles = host.html.attr(body, "a[href*='/manga/'][title]", "title")
  local covers = host.html.attr(body, "a[href*='/manga/'][title] img", "src")
  local out = {}
  for i, href in ipairs(hrefs) do
    local slug = find1(href, [[/manga/([^/]+)]])
    if slug and slug ~= "" and not slug:find("^%?") then
      out[#out + 1] = {
        id = slug,
        name = clean(titles[i] or ""),
        cover = covers[i] or "",
        url = href,
      }
    end
  end
  local seen, uniq = {}, {}
  for _, it in ipairs(out) do
    if not seen[it.id] then seen[it.id] = true; uniq[#uniq + 1] = it end
  end
  return uniq
end

local function parse_cards(body)
  local hrefs = host.html.attr(body, "a[href*='/manga/'][title]", "href")
  local titles = host.html.attr(body, "a[href*='/manga/'][title]", "title")
  local covers = host.html.attr(body, "a[href*='/manga/'][title] img", "src")
  local out = {}
  for i, href in ipairs(hrefs) do
    local slug = find1(href, [[/manga/([^/]+)]])
    if slug and slug ~= "" and not slug:find("^%?") then
      out[#out + 1] = {
        id = slug,
        name = clean(titles[i] or ""),
        cover = covers[i] or "",
        url = href,
      }
    end
  end
  local seen, uniq = {}, {}
  for _, it in ipairs(out) do
    if not seen[it.id] then seen[it.id] = true; uniq[#uniq + 1] = it end
  end
  return uniq
end

return { BASE=BASE, AJAX=AJAX, OPTS=OPTS, http_req=http_req, find1=find1, clean=clean, first=first, manga_url=manga_url, parse_cards=parse_cards }
