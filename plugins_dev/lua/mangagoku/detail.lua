local c = require("common")
local SITE = c.SITE
local OPTS = c.OPTS
local http_req = c.http_req
local get_json = c.get_json
local find1 = c.find1
local clean = c.clean
local epoch = c.epoch
local slug_of = c.slug_of
local search_page = c.search_page
local page_memo = c.page_memo
local paging = c.paging
local book_genres = c.book_genres
local book_description = c.book_description
return function(params)
  local id = slug_of(params.id or "")
  if id == "" then return { ok = true, data = {} } end
  local body, err = http_req("/manga/" .. host.url_encode(id))
  if not body or body == "" then return { ok = false, error = err } end

  -- The RSC payload carries several ld+json blobs (Organization first, Book
  -- later); anchor on "@type":"Book" instead of taking the first blob.
  local genres, authors = book_genres(body), {}
  local author = find1(body, [[(?s)"@type":"Book".*?"author":.*?"name":"([^"]+)"]])
  if author then
    author = clean(author)
    local seen = {}
    for _, a in ipairs(authors) do seen[a] = true end
    if not seen[author] then authors[#authors + 1] = author end
  end

  local name = clean(find1(body, [[<meta property="og:title" content="([^"]+)]]) or "")
  name = name:gsub("%s*|%s*MangaGoku%s*$", "")
  local status = clean(find1(body, [[(?s)Status</span><span[^>]*>([^<]+)</span>]]) or "")
  if status == "" then status = clean(find1(body, [[text%-green%-400">([^<]+)</span>]]) or "") end

  return { ok = true, data = {
    id = id,
    name = name,
    cover = find1(body, [[<meta property="og:image" content="([^"]+)]]) or "",
    description = book_description(body),
    status = host.text.normalize_status(status),
    tags = genres,
    authors = authors,
    artists = {},
    release_at = 0,
    url = SITE .. "/manga/" .. id,
  } }
end
