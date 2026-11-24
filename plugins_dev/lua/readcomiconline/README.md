# Read Comics Online plugin — RETIRED (2026-11-24)

Not linked into `data/plugins/`: the site's Cloudflare challenge needs a headed
browser or a frequently refreshed `cf_clearance`, so the port was retired from
the active set. Source kept here for reference; the one already-saved title
stays in the library and remains readable from cache (disabled = cache-only).
The refresh helper `scripts/refresh-rco-cookie.py` is kept with the source but
is no longer part of normal operation. Nothing below runs in the daemon today.

Lua scraper for `readcomicsonline.ru`. All pages are server-rendered Blade HTML,
so no browser/CDP is used at runtime.

## Cloudflare clearance

The site issues a Cloudflare JS challenge to plain clients. The plugin sends
`cf_clearance` / session cookies and the exact bound User-Agent; both are
secrets stored in the gitignored `config.json` (`cookie`, `user_agent`), never
in the distributed `main.json`. When fetches fail with `upstream status 403`
or `Cloudflare challenge — run scripts/refresh-rco-cookie.py`, mint a fresh
pair from the daemon's own Chrome (it clears the challenge automatically):

```sh
python3 scripts/refresh-rco-cookie.py   # needs websocket-client
./run-cppIsekai.sh restart              # Lua is compiled at boot
```

The script opens and closes a throwaway tab, copies only the bound cookie and
UA, and never prints their values. No host code changes needed.

## Methods

- `search` — `GET /advanced-search?name=…`, returns all matches, no pagination.
- `detail` — `GET /comic/<slug>`: name, cover, status, genres, synopsis.
- `chapters` — chapter links from the detail page, canonical number ascending.
- `pages` — every `<img src>` inside the reader's `<div id="reader-all">`.
- `genres` — the site's own `category` select values (publishers) as strings;
  the first selection is mapped back to its numeric id for searches.

Images on `cdn.readcomicsonline.ru` are covered by the host's domain-matched
Referer resolution; no per-plugin `image_referer` override is required.
