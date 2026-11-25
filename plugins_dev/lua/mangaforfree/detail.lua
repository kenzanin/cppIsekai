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
  local body, err = http_req(manga_url(id))
  if not body or body == "" then return { ok = false, error = err } end
  local blob = host.text.json_blob(body, "application/ld+json") or ""
  local ok, ld = pcall(json.decode, blob)
  local doc = ok and type(ld) == "table" and ld or {}
  local tags = {}
  for _, g in ipairs(host.html.select(body, ".genres-content a") or {}) do
    g = clean(g)
    if g ~= "" then tags[#tags + 1] = g end
  end
  -- The page has several "summary-content" blocks (Release, Status, Author),
  -- so anchor on labels instead of taking the first one.
  -- Release is block 1, Status is block 2 in the Madara summary panel.
  local blocks = host.html.select(body, ".post-status .summary-content") or {}
  local status = clean(blocks[2] or "")
  return { ok = true, data = {
    id = id,
    name = clean(first(host.html.attr(body, "meta[property='og:title']", "content")) or doc.name or ""),
    cover = first(host.html.attr(body, "meta[property='og:image']", "content")) or "",
    description = clean(first(host.html.select(body, ".description-summary .summary-content p")) or doc.description or ""),
    status = host.text.normalize_status(status),
    tags = tags,
    authors = clean(first(host.html.select(body, ".author-content a")) or ""),
    artists = {},
    url = manga_url(id),
  } }
end
