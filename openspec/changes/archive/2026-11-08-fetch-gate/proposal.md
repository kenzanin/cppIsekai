# Proposal

## Why

`curl_easy_impersonate` is called from two places (plugin.cpp `fetch_sync` :261 and the legacy lua path :562) and the effective profile is pinned at plugin load time — changing the TLS fingerprint requires a restart. Cookies do not persist anywhere. User steer 2026-11-08 (OKF `decisions/fetch-profile-gate`, `decisions/plugin-cookie-jars`): one centralized gate, dynamic per-call profile, per-plugin persistent jar.

## What Changes

- **New module `src/fetch/`**: `fetch::Request{url, timeout_ms, profile?, cookie_jar?, method, body, headers...}` + `fetch::sync(Request, proxy)` — the single `curl_easy_impersonate` call site in the tree; sink/retry-ponytail/`applyProxy` move here; the :561-area duplicate is folded away.
- **Dynamic profile**: plugin opts (lua/js/wasm) accept a per-call `profile`; resolution `opts > plugin.fetch_profile (ini)`; **live default** — config is read at call time via snapshot, so `POST /config` takes effect without reload; unknown target => warn + fallback to default.
- **Cookie jar**: a `cookie_jar` (path) option on the gate; per-plugin jar helper `data/plugin-state/<id>.cookies.txt`; native `CURLOPT_COOKIEFILE/JAR`; off by default (legacy behavior when absent).
- **Firefox family (user steer 2026-11-08, in this phase)**: the chrome-family lib is joined by `libcurl-impersonate-firefox`, loaded `dlopen(RTLD_LOCAL)` + per-family symbol vtable — two independent libcurl copies, one gate; firefox targets read from the lib's own list (probe, task 1.3).
- Out of scope: the search read-model cache (change `search-cache`), cookie UI.

## Capabilities

### New Capabilities

(none — host fetch behavior; requirements land in the `plugin-system` delta)

### Modified Capabilities

- `plugin-system`: adds requirements **Per-call impersonation profile** and **Persistent per-plugin cookie jar** (ADDED only; no existing requirement changes).

## Impact

- Files: `src/fetch/fetch.{hpp,cpp}` (new), `src/plugin/plugin.{hpp,cpp}` (consumer + duplicate removal), `tests/test_plugin.cpp`, `CMakeLists.txt` (target sources+link), smoke fixture.
- No new dependencies; chrome-family targets per the OKF probe (chrome99-116/edge99-101/safari15); firefox per its own lib (probe 1.3) — same package family, no new dep.
- Risk: existing http behavior must stay identical (parity) — guarded by existing tests + smoke; touches the hot path of every plugin.
