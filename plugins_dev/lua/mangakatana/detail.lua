local c = require("common")
local BASE = c.BASE
local opts = c.opts
local http_req = c.http_req
local find1 = c.find1
local slug_of = c.slug_of
local url_for = c.url_for
local chapter_url = c.chapter_url
local page_urls = c.page_urls
return function(params)
  local manga_id = slug_of(params.id or "")
  local empty = { id = manga_id, name = "", cover = "", status = "unknown", tags = {}, release_at = 0,
    description = "", authors = {}, artists = {}, url = url_for("manga", manga_id) }

  local body, err = http_req(url_for("manga", manga_id))
  if err ~= nil then return { ok = false, error = err } end

  -- #related sits inside #single_book and repeats the card shape, so every
  -- lookup below is scoped to stay off it.
  local tags = {}
  for _, name in ipairs(host.html.select(body, "#single_book .genres a")) do
    tags[#tags + 1] = host.text.trim(name)
  end

  -- The description is one <p> whose line breaks are <br> tags; the text-node
  -- xpath splits exactly where the <br>s are (find_text would glue the words).
  local description = table.concat(
    host.html.xpath("//div[@id='single_book']//div[@class='summary']/p/text()", body) or {}, "\n")

  local data = empty
  data.name = host.html.select(body, "h1.heading")[1] or ""
  local author = host.html.select(body, "#single_book .authors a.author")[1]
  data.cover = host.html.attr(body, "#single_book .cover img", "src")[1] or ""
  -- The status cell reads "Completed" / "Ongoing" / "Cancelled"; the host
  -- owns the canonical vocabulary.
  data.status = host.text.normalize_status(host.html.select(body, "#single_book div.value.status")[1] or "")
  data.tags = tags
  data.description = description
  if author and author ~= "" then data.authors = { host.text.trim(author) } end
  return { ok = true, data = data }
end
