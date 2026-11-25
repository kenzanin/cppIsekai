local c = require("common")
local SITE = c.SITE
local get = c.get
local detail_body = c.detail_body
local find1 = c.find1
local find_all = c.find_all
local clean = c.clean
local epoch = c.epoch
local cards = c.cards
local last_page = c.last_page
local genre_slug = c.genre_slug
local page_cache = c.page_cache
local page_info = c.page_info
return function(params)
  local slug = tostring(params.id or "")
  if slug == "" then
    return { ok = true, data = {} }
  end
  local body, err = detail_body(slug)
  if not body then
    return { ok = false, error = err or "manhwato: detail failed" }
  end
  return {
    ok = true,
    data = {
      id = slug,
      name = clean(find1(body, [[<h1 class="mt-dhero__title">([^<]+)]]) or slug),
      cover = find1(body, [[<meta property="og:image" content="([^"]+)"]]) or "",
      description = clean(find1(body, [[(?s)<div class="about">(.*?)</div>]])),
      status = host.text.normalize_status(clean(find1(body, [[<dt>Status</dt><dd><span[^>]*></span>([^<]+)]]) or "")),
      authors = { clean(find1(body, [[<dt>Author</dt><dd><a[^>]*>([^<]+)]]) or "") },
      tags = (function()
        local out = {}
        local block = find1(body, [[(?s)<ul class="mt-pills mt-pills--muted">(.*?)</ul>]]) or ""
        for _, m in ipairs(find_all(block, [[<a[^>]*>([^<]+)</a>]])) do
          out[#out + 1] = clean(m[2])
        end
        return out
      end)(),
      genres = (function()
        local out = {}
        for name in tostring(find1(body, '(?s)"genre":\\s*\\[(.*?)\\]') or ""):gmatch('"([^"]+)"') do
          out[#out + 1] = clean(name)
        end
        return out
      end)(),
      url = SITE .. "/manhwa/" .. slug,
    },
  }
end
