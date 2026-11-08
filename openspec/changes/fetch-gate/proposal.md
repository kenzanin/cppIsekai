# Proposal

## Why

`curl_easy_impersonate` is called from two places (plugin.cpp `fetch_sync` :261 and the legacy lua path :562) and the effective profile is pinned at plugin load time — changing the TLS fingerprint requires a restart. Cookies do not persist anywhere. User steer 2026-11-08 (OKF `decisions/fetch-profile-gate`, `decisions/plugin-cookie-jars`): one centralized gate, dynamic per-call profile, per-plugin persistent jar.

## What Changes

- **New module `src/fetch/`**: `fetch::Request{url, timeout_ms, profile?, cookie_jar?, method, body, headers...}` + `fetch::sync(Request, proxy)` — the single `curl_easy_impersonate` call site in the tree; sink/retry-ponytail/`applyProxy` move here; the :561-area duplicate is folded away.
- **Dynamic profile**: plugin opts (lua/js/wasm) accept a per-call `profile`; resolution `opts > plugin.fetch_profile (ini)`; **live default** — config is read at call time via snapshot, so `POST /config` takes effect without reload; unknown target => warn + fallback to default.
- **Cookie jar**: a `cookie_jar` (path) option on the gate; per-plugin jar helper `data/plugin-state/<id>.cookies.txt`; native `CURLOPT_COOKIEFILE/JAR`; off by default (legacy behavior when absent).
- Out of scope: firefox target (needs the separate `-firefox` .so — phase 2), the search read-model cache (change `search-cache`), cookie UI.

## Capabilities

### New Capabilities

(none — host fetch behavior; requirements land in the `plugin-system` delta)

### Modified Capabilities

- `plugin-system`: adds requirements **Per-call impersonation profile** and **Persistent per-plugin cookie jar** (ADDED only; no existing requirement changes).

## Impact

- Files: `src/fetch/fetch.{hpp,cpp}` (new), `src/plugin/plugin.{hpp,cpp}` (consumer + duplicate removal), `tests/test_plugin.cpp`, `CMakeLists.txt` (target sources+link), smoke fixture.
- No new dependencies; targets follow the OKF probe (chrome99-116/edge99-101/safari15).
- Risk: existing http behavior must stay identical (parity) — guarded by existing tests + smoke; touches the hot path of every plugin.
