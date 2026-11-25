local c = require("common")
local BASE = c.BASE
local CDN = c.CDN
local HEADERS = c.HEADERS
local GENRE_SLUGS = c.GENRE_SLUGS
local normalize_status = c.normalize_status
local image_url = c.image_url
local get_json = c.get_json
local epoch_ms = c.epoch_ms
local strip_html = c.strip_html
return function()
  local names = require("genres").names
  return { ok = true, data = names }
end
