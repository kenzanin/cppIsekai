# Read Comics Online plugin

Lua scraper for `readcomicsonline.ru`. All pages are server-rendered Blade HTML,
so no browser/CDP is used at runtime.

## Cloudflare clearance

The site issues a Cloudflare JS challenge to plain clients. The plugin sends
`cf_clearance` / session cookies and the exact bound User-Agent captured in
`tmp/readcomiconline.har`. Both are secrets: they live in the gitignored
`config.json` (`cookie`, `user_agent`), never in the distributed `main.json`.
They expire: when fetches start failing with `upstream status 403`, copy a
fresh request `Cookie` header and `User-Agent` from an authenticated browser
session into that file and restart the daemon (plugins are compiled at boot).
No host code changes needed.

## Methods

- `search` — `GET /advanced-search?name=…`, returns all matches, no pagination.
- `detail` — `GET /comic/<slug>`: name, cover, status, genres, synopsis.
- `chapters` — chapter links from the detail page, canonical number ascending.
- `pages` — every `<img src>` inside the reader's `<div id="reader-all">`.
- `genres` — the site's own `category` select values (publishers) as strings;
  the first selection is mapped back to its numeric id for searches.

Images on `cdn.readcomicsonline.ru` are covered by the host's domain-matched
Referer resolution; no per-plugin `image_referer` override is required.
