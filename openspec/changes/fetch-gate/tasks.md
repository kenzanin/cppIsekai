# Tasks

## 1. Gate module

- [ ] 1.1 `src/fetch/fetch.{hpp,cpp}`: `Request`/`sync`, sink+retry+applyProxy moved, single `curl_easy_impersonate`; plugin.cpp becomes a consumer; full `ctest` green (identical behavior)
- [ ] 1.2 `profile` opt threading lua/js/wasm + live default (config snapshot read at call time; `POST /config` effective without reload); unknown target => warn + fallback
- [ ] 1.3 Firefox family: probe `libcurl-impersonate-firefox` presence + target list; `dlopen(RTLD_LOCAL)` loader + per-family vtable in `src/fetch/`; `firefox*` routing + family-map unit test (network-free); absent lib => BLOCKED with install instructions

## 2. Cookie jar

- [ ] 2.1 `cookie_jar` on the gate (`CURLOPT_COOKIEFILE/JAR`) + `data/plugin-state/<id>.cookies.txt` helper; off by default; failure => non-fatal warn
- [ ] 2.2 Plugin opt to enable cookies; short doc in `docs/` or the gate header comment

## 3. Verify

- [ ] 3.1 Unit: invalid-target fallback; tmp-dir jar round-trip; two-jar isolation; full ctest green zero new warnings; `openspec validate --specs`; optional live smoke via `PLUGIN_SMOKE_URL`
