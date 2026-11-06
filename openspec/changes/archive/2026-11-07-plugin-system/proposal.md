# Proposal

## Why

The viewer is currently fed by 48 hardcoded seed rows. Every meaningful page (library with real manga, detail with chapters, enrichment, update checks) depends on pulling data from manga websites, and the architecture decision (`architecture/manga-viewer-core`) assigns that to site plugins orchestrated by the host. The plugin system is the gate to all remaining features, so it comes before the FTS5 search page (search over real data needs plugins first).

## What Changes

- New plugin directory layout: `data/plugins/<id>/` with one folder per plugin containing `main.lua` (entry; later `main.wasm`/`main.js`/`main.py`), `main.json` (manifest: name, version, runtime, capabilities, api version, entry), and an optional logo file (`logo.png|svg|jpg|avif`) served at `/plugin/<id>/logo` and shown wherever a plugin name appears.
- Manifest declared path: `plugins=data/plugins` in `data/cppisekai.ini` (SimpleIni config already exists).
- Plugin ABI as a data contract, not native signatures: host calls plugins with a JSON envelope (`{"api":1,"method":"search","params":{...}}`) and accepts `{"ok":true,"data":...}` / `{"ok":false,"error":"..."}`. Plugin manifest carries `"api": 1`; host rejects plugins whose api version is newer, tolerates older best-effort (OpenAPI-style versioning).
- First host-tools binding for Lua via sol2: `http.fetch(url, {timeout_ms?, profile?})` backed by curl-impersonate (empty options = global `.ini` defaults `fetch_timeout_ms` / `fetch_profile`), plus `log`, `now`.
- Light Lua sandbox: whitelisted stdlibs (`base, string, table, math, json`), limited `io` (`io.lines`/`io.read` only — no write), `os` and `debug` blocked; instruction-count timeout hook per call.
- Plugin loader: scans the plugin dir at startup, parses manifests, registers plugins in the DB `plugin` table; dev endpoint `POST /plugins/reload` to rescan without restart.
- First real plugin: `mangadex` (JSON API, no HTML parsing needed yet), capabilities `search` + `detail` + `chapters`, normalizing output to the MangaDex-canonical tags/status and unix-time contract.
- Import path: plugin search results can be added to the library (dedup check at add-time per the FTS5/dedup decision — the FTS5 index itself lands with the search-page change; this change records plugin ids on titles).

## Capabilities

### New Capabilities
- `plugin-system`: plugin discovery/manifest/loading, the plugin ABI envelope and api versioning, the Lua sandbox rules, and the `POST /plugins/reload` behavior.
- `plugin-host-tools`: the host-tools contract plugins can call (`http.fetch` with timeout/profile defaults, `log`, `now`) and their error reporting.
- `mangadex-plugin`: the mangadex plugin's observable behavior — search, detail, chapters, MangaDex-canonical output, unix-time dates.

### Modified Capabilities
(none — no spec-level behavior exists yet for library import; this change adds plugin ids to titles as implementation, with import UX specified when the search page lands)

## Impact

- New `src/plugin/` scope folder (loader + sandbox + binding), entry wired in `src/main.cpp` and used from `src/http/`.
- `Store` gains plugin registration queries (all SQL stays in `Store`).
- Runtime deps already vendored/linked: sol2 + lua5.4 (system), curl-impersonate `.so` (vendored, `curl_easy_impersonate` needs the extern declaration noted in `references/dependency-api`).
- No new dependencies this change; lexbor (`dom.select`) is deliberately deferred until an HTML-scraping plugin needs it.
- Templates: sidebar plugin list shows logos from the plugin dir.
- Config: `plugins=`, `fetch_timeout_ms=`, `fetch_profile=` keys read from `data/cppisekai.ini`.
