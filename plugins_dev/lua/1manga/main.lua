-- 1Manga.co plugin (https://1manga.co): HTML scrape, api 1.
-- The site's public GraphQL (api.mghcdn.com/graphql/mn03) is locked to its own
-- client since the 2025 rewrite: every key answers 403 "app does not have
-- permission". What it does serve is fully server-rendered HTML -- search
-- results, detail metadata, the whole chapter list and every page image
-- (imgx.mghcdn.com, plain GET, no referer, no crypto) -- so this port scrapes
-- that. Class names are hashed per deploy, so selectors stay structural
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
  if fn == nil then return { ok = false, error = "unknown method " .. tostring(req.method) } end
  return fn(req.params or {})
end
