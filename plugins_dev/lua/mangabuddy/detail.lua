local c = require("common")
local BASE = c.BASE
local helpers = c.helpers
local genres = c.genres
local find1 = c.find1
local bare_slug = c.bare_slug
local label_value = c.label_value
local epoch = c.epoch
return function(params)
  local id = tostring(params.id or "")
  local html = helpers.http_get(BASE .. "/series/" .. id)
  if not html then
    return { ok = false, error = "mangabuddy: detail fetch failed" }
  end

  local title = find1(html, [[<meta property="og:title" content="([^"]*)"]]) or ""
  title = title:gsub("^Read%s+", "")
  title = title:gsub("%s*|%s*MangaBuddy%s*$", "")
  title = title:gsub("%s+Online$", "")
  -- multi-encoded metas ("&amp;amp;039;" seen, # eaten by their CMS): the
  -- host helper decodes to a fixpoint and sweeps #-less numeric refs.
  title = host.text.clean(title)

  local cover = find1(html, [[<meta property="og:image" content="([^"]*)"]]) or ""

  local desc = find1(html, [[<meta name="description" content="([^"]*)"]]) or ""
  desc = desc:gsub("^Read%s+[^%.]+%.%s*", "")
  desc = desc:gsub("%s*Read the latest chapters online for free at MangaBuddy%.?%s*$", "")
  desc = host.text.clean(desc)

  local status = label_value(html, "Status", "/series?status=")
  local author = label_value(html, "Author", "/author/")
  if author == "Unknown" then
    author = ""
  end

  -- genre chips: href="/genre/{slug}"; the label is slug-cased, so retitle
  -- each word (re2.replace has no function replacement)
  local tags, seen_g = {}, {}
  for _, r in ipairs(host.re2.find_all([[href="/genre/([a-z0-9-]+)"]], html)) do
    local g = r[2]
    if g and g ~= "" and not seen_g[g] then
      seen_g[g] = true
      local label = g:gsub("%-", " ")
      label = label:gsub("(%a)([%w']*)", function(a, b)
        return a:upper() .. b
      end)
      tags[#tags + 1] = label
    end
  end

  return {
    ok = true,
    data = {
      id = id,
      name = title,
      cover = cover,
      status = helpers.normalizeStatus(status),
      tags = tags,
      release_at = 0, -- the page carries no machine-readable series date
      description = desc,
      authors = author ~= "" and { author } or {},
      url = BASE .. "/series/" .. id,
    },
  }
end
