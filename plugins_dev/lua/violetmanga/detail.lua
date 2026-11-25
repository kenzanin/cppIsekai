local c = require("common")
local BASE = c.BASE
local http_req = c.http_req
local sel1 = c.sel1
return function(params)
  local slug = params.id or ""
  local empty = { id = slug, name = "", cover = "", status = "unknown", tags = {}, release_at = 0,
    description = "", authors = {}, artists = {}, url = BASE .. "/comics/" .. slug .. "/" }
  if slug == "" then return { ok = true, data = empty } end

  local body, err = http_req(BASE .. "/comics/" .. slug .. "/")
  if err ~= nil then return { ok = false, error = err } end

  local data = empty
  data.name = host.text.unescape(sel1(body, "h1.entry-title"))
  local author = host.text.unescape(sel1(body, ".imptdt span.author i"))
  if author ~= "" then data.authors = { author } end
  data.tags = host.html.select(body, ".genres-container .mgen a")
  data.status = host.text.normalize_status(sel1(body, ".info-half .status i"))
  data.cover = host.html.attr(body, ".thumb img", "src")[1] or ""

  -- description block; Lua patterns span newlines natively (no (?s) needed)
  local dblock = body:match('class="summary[^"]*">.-class="entry%-content[^"]*">(.-)</div>')
    or body:match('class="entry%-content[^"]*">(.-)</div>') or ""
  data.description = host.text.unescape(host.text.strip_html(dblock)):gsub("%s+", " ")

  return { ok = true, data = data }
end
