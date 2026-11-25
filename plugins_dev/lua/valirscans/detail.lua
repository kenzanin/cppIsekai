local c = require("common")
local BASE = c.BASE
local http_req = c.http_req
local find1 = c.find1
local is_abs_url = c.is_abs_url
local jsonld_book = c.jsonld_book
local flight_status = c.flight_status
return function(params)
  local manga_id = params.id or ""
  local empty = { id = manga_id, name = "", cover = "", status = "unknown", tags = {}, release_at = 0,
    description = "", authors = {}, artists = {}, url = BASE .. "/series/comic/" .. manga_id }
  if manga_id == "" then return { ok = true, data = empty } end

  local html, err = http_req(BASE .. "/series/comic/" .. manga_id)
  if err ~= nil then return { ok = false, error = err } end

  local data = empty
  local book = jsonld_book(html)
  if book then
    data.name = book.name or ""
    if type(book.author) == "table" and book.author.name then
      data.authors = { book.author.name }
    elseif type(book.author) == "string" and book.author ~= "" then
      data.authors = { book.author }
    end
    data.description = book.description or ""
    local img = book.image or ""
    if img ~= "" then
      data.cover = is_abs_url(img) and img or BASE .. img
      -- status: anchor on the cover path (relative) in the flight payload
      local status = flight_status(html, img)
      if status ~= "" then data.status = host.text.normalize_status(status) end
    end
    if type(book.genre) == "table" then
      local gs = {}
      for _, g in ipairs(book.genre) do gs[#gs + 1] = g end
      data.tags = gs
    end
  end

  -- Fallbacks for anything JSON-LD missed (or if the block was absent)
  if data.name == "" then
    data.name = host.text.unescape(find1(html, [[<meta property="og:title" content="([^"]*)"]]) or "")
  end
  if data.cover == "" then
    data.cover = find1(html, [[<meta property="og:image" content="([^"]*)"]]) or ""
  end
  if data.status == "unknown" then
    -- retry the flight payload with whatever origin the cover URL carries
    local origin = find1(data.cover, [[(https?://[^/]+)?(/uploads/series/[^"]+)]]) or data.cover
    local st = flight_status(html, origin)
    if st ~= "" then data.status = host.text.normalize_status(st) end
  end
  return { ok = true, data = data }
end
