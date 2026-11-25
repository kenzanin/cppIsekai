-- 1Manga.co plugin (https://1manga.co): HTML scrape, api 1.
-- The site's public api.mghcdn.com GraphQL is locked to its own client (403
-- "app does not have permission"), but the SAME-ORIGIN /graphql answers with
-- a minted mhub_access cookie -- pages.lua uses that (the chapter HTML
-- server-renders only the first 6 images). What the HTML does serve fully is
-- search results, detail metadata and the whole chapter list, so those stay
-- scrapes. Class names are hashed per deploy, so selectors stay structural
-- (bootstrap classes, labels, anchors) and never key on _3SlhO-style names.
--
-- The source is split by purpose; this file only wires the handlers.
local BASE = host.plugin.param("site_url") or "https://1manga.co"

local http = require("http")
local ctx = {
  BASE = BASE,
  get = http.get,
  text = require("text"),
}

local methods = {
  search = require("search")(ctx),
  detail = require("detail")(ctx),
  chapters = require("chapters")(ctx),
  pages = require("pages")(ctx),
  genres = require("genres")(ctx),
}

function handle(req)
  local fn = methods[req.method]
  if fn == nil then
    return { ok = false, error = "unknown method " .. tostring(req.method) }
  end
  return fn(req.params or {})
end
