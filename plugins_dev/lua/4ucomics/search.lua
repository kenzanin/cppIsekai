local c = require("common")
local API = c.API
local STATUS = c.STATUS
local proxy_img = c.proxy_img
local status_of = c.status_of
local get_json = c.get_json
local number_of = c.number_of
return function(params)
  local q = params.title or params.query or params.q or ""
  if q == "" then return { ok = false, error = "4ucomics: empty query" } end
  local d, err = get_json("/search/" .. host.url_encode(q) .. "?adult=1")
  if err then return { ok = false, error = err } end
  local out = {}
  for _, h in ipairs(d.hits or {}) do
    out[#out + 1] = {
      id = h.slug,
      name = h.title or "",
      cover = proxy_img(h.cover or ""),
      status = status_of(h.statuses),
    }
  end
  -- UI contract: {items, total} envelope, not a bare array (search_script run()).
  return { ok = true, data = { items = out, total = #out } }
end
