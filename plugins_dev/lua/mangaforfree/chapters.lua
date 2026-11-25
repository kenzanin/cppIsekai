local c = require("common")
local BASE = c.BASE
local AJAX = c.AJAX
local OPTS = c.OPTS
local http_req = c.http_req
local find1 = c.find1
local clean = c.clean
local first = c.first
local manga_url = c.manga_url
local parse_cards = c.parse_cards
return function(params)
  local id = find1(params.id or "", [[/manga/([^/]+)]]) or tostring(params.id or "")
  local detail, err = http_req(manga_url(id))
  if not detail or detail == "" then
    return { ok = false, error = err }
  end
  local post_id = find1(detail, [["manga_id":"([0-9]+)"]])
  if not post_id then
    return { ok = true, data = {} }
  end
  local body = http_req(AJAX, {
    method = "POST",
    body = "action=manga_get_chapters&manga=" .. post_id,
    headers = { ["Content-Type"] = "application/x-www-form-urlencoded", ["X-Requested-With"] = "XMLHttpRequest" },
  }) or ""
  local hrefs = host.html.attr(body, "li.wp-manga-chapter a[href]", "href") or {}
  local labels = host.html.select(body, "li.wp-manga-chapter a[href]") or {}
  local dates = host.html.select(body, "li.wp-manga-chapter .chapter-release-date i") or {}
  local out = {}
  for i, href in ipairs(hrefs) do
    local title = clean(labels[i] or "")
    if not title:lower():find("raw", 1, true) then
      local slug = find1(href, [[/manga/[^/]+/([^/]+)]])
      if slug then
        out[#out + 1] = {
          id = id .. ":" .. slug,
          number = host.text.chapter_num(title) or 0,
          title = title,
          release_at = host.text.date_epoch(clean(dates[i] or "")) or 0,
          pages = 0,
          url = href,
        }
      end
    end
  end
  table.sort(out, function(a, b)
    return a.number < b.number
  end)
  return { ok = true, data = out }
end
