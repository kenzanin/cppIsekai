-- KaliScan (kaliscan.me): search results come from a backend HTML fragment,
-- chapters from the detail page, page images from a chapterServer fragment.
-- Port of goIsekai examples/plugins/lua/kaliscan; host.re2.* replaces
-- host.regex.* (pattern-first args), host.http.fetch replaces get_body.
-- Envelope { ok, data | error }; canonical manga fields per the mangadex plugin.
local BASE = "https://kaliscan.me" -- manifest site_url; source notes flag the domain as dead, kept for parity

local genres = require("genres")
local util = require("util")

-- One search walks at most this many backend fragment pages: each is a
-- separate request, and out-of-range pages wrap to page 1, so the walk below
-- stops as soon as a page adds nothing new to the deduped set.
local MAX_RESULT_PAGES = 5

-- http request -> (body, err). 200-or-nothing, the anisascans http_req shape.
local http_req = host.http.fetch_body -- (body, err); nil unless 200 (plugin-host-promotion).

-- ── search ─────────────────────────────────────────────────────────────────

-- ── detail / chapters ──────────────────────────────────────────────────────

-- ── pages ──────────────────────────────────────────────────────────────────
-- Chapter id "slug:chapter-N" -> /manga/slug/chapter-N; that page carries
-- chapterId=N, and the images live on the chapterServer fragment. The request
-- needs the chapter Referer; returned pages are plain URL strings (per-page
-- image headers are a host-side concern, see the site-origin Referer fallback).

-- ── genres ─────────────────────────────────────────────────────────────────

return { BASE = BASE, genres = genres, util = util, MAX_RESULT_PAGES = MAX_RESULT_PAGES, http_req = http_req }
