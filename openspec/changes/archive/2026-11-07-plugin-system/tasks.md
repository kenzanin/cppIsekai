# Tasks

## 1. Plugin config & scaffolding

- [x] 1.1 Add `plugins=`, `fetch_timeout_ms=`, `fetch_profile=` keys to the SimpleIni load in `src/main.cpp` (defaults: `data/plugins`, 30000, `chrome116`); verify by running with a custom `.ini` and observing the log line that prints resolved values
- [x] 1.2 Create `src/plugin/` scope folder with `plugin.hpp`/`plugin.cpp` skeleton (PluginInfo struct: id, name, version, runtime, capabilities, api, dir path; empty `load_all()`), wire into CMakeLists and `src/main.cpp`; verify `cmake --build build` succeeds and `ctest` stays green

## 2. Manifest & loader

- [x] 2.1 Parse `main.json` manifests with glaze (name, id, version, runtime, capabilities, api); skip-with-warning on parse error, missing entry file, or unknown runtime — verify with a unit test covering valid manifest, broken JSON, and missing fields (test fixture plugins under `tests/fixtures/plugins/`)
- [x] 2.2 Implement `load_all()`: scan `plugins` dir, dedupe ids first-wins-with-warning, call into `Store` to upsert `(id, name, version, capabilities)`; verify via doctest: two fixture plugins load, a duplicate-id pair logs and keeps the first, Store reports the registered rows
- [x] 2.3 Create Lua runtime slot: per-plugin `sol::state` creation with sandbox whitelist (base/string/table/math/json + io.lines/io.read shim only; os, debug, package never opened) and instruction-count timeout hook; verify with a doctest: a good fixture plugin calls `handle`, `os.execute` raises a plugin error, an infinite-loop fixture times out and returns a timeout error
- [x] 2.4 Implement ABI envelope dispatch: build `{api, method, params}` table, call the plugin's `handle`, accept `{ok=true,data=...}` / `{ok=false,error=...}`, reject manifest api > host api at load; verify with doctest fixtures: ok round-trip, error round-trip, malformed return treated as failure, future-api plugin skipped with version-named log

## 3. Host tools binding

- [x] 3.1 Implement `http.fetch(url, options)` on the plugin pool via curl-impersonate (`CURLOPT_TIMEOUT_MS` from options or ini default; `curl_easy_impersonate` with profile from options or ini default, using the extern declaration) returning `{status, body, headers}`; verify with a doctest against a local fixture HTTP response file plus a live `https://api.mangadex.org/ping` smoke: defaults used when options empty, per-call override honored, unreachable host returns detectable failure
- [x] 3.2 Implement `log.info/error` (tagged with plugin id, routed through the spdlog queue sink so messages reach stdout and the websocket log page) and `now()` returning integer unix seconds; verify via doctest that a fixture plugin's log line appears in LogBuffer snapshot and `now()` matches `std::time(nullptr)` within 2 s

## 4. Routes & UI integration

- [x] 4.1 Add `POST /plugins/reload` route that re-runs `load_all()` and returns the plugin count; verify with curl: add a fixture plugin folder, reload, see the count change without restart
- [x] 4.2 Add `/plugin/<id>/logo` route serving the logo file with content type by extension (png/svg/jpeg/avif), 404 when absent; verify with curl -I for each extension fixture and a missing-logo 404
- [x] 4.3 Render plugin logos next to names in the sidebar plugin list (top.mustache partial); verify visually with a browser check on `/` and confirm the img src resolves 200

## 5. Mangadex plugin

- [x] 5.1 Create `data/plugins/mangadex/` with `main.json` (api 1, runtime lua, capabilities search/detail/chapters) and `logo.png`; verify the loader registers it and `/plugin/mangadex/logo` serves the image
- [x] 5.2 Implement `main.lua` search/detail/chapters against `https://api.mangadex.org` using only `http.fetch` + json + table/string libs, returning MangaDex-canonical fields and integer `release_at`; verify with a smoke script call: search "solo leveling" returns ok-shaped canonical items, chapters return integer timestamps, upstream failure returns the error shape
- [x] 5.3 Exercise the full path from the running host (UI or curl to a debug route): search results render, an upstream outage surfaces as a logged plugin error without a 5xx from the host; record the observed behavior in the change notes

## 6. Integration verification

- [x] 6.1 Run `cmake --build build && (cd build && ctest)` — all existing Store tests plus new plugin tests pass
- [x] 6.2 Full manual smoke: start host, confirm startup scan log lists mangadex, reload endpoint works, logo route 200, log page shows plugin-tagged lines; then `git diff --check` and commit

### 5.3 observed behavior (2026-11-06)

- Startup scan log lists mangadex (`plugin loaded: mangadex v1.0.0 (api 1, caps: search,detail,chapters)`).
- `POST /plugins/reload` returns `{"count":N}`; adding/removing a folder changes N without restart (verified with temporary logo fixture plugins, 1 -> 5 -> 1).
- `/plugin/mangadex/logo` -> 200 `image/png`; unknown plugin logo -> 404.
- `/plugin/mangadex/search?query=solo+leveling` returns ok-shaped canonical items (id/name/cover/status/tags/release_at); chapters return integer unix `release_at` (ISO-8601 parsed arithmetically — `os` is not in the plugin sandbox).
- curl to `/plugin/<id>/<method>` always answers 200 with the ABI envelope; upstream outages surface as `{"ok":false,"error":...}` plus a `[plugin:mangadex]` spdlog warning (visible on the log page), never a 5xx from the host.
- Known quirk: the vendored curl-impersonate build intermittently fails the TLS handshake against MangaDex's LB (ECONNRESET); `http.fetch` retries once on `CURLE_SSL_CONNECT_ERROR` with a fresh handle, which removed it in practice.
- MangaDex serves a bot-challenge HTML page to browser UAs on API endpoints; the plugin sends an app UA (`cppIsekai/0.1`) via `http.fetch` `user_agent` option and gets clean JSON.
