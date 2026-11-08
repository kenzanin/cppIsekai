# Design

## Context

Current state & constraints: two impersonate call sites (`fetch_sync` in plugin.cpp and the legacy lua curl block), profile from `conf_.fetch_profile` captured in closures at plugin load; sink/retry/`applyProxy` inline. Decisions locked in OKF `decisions/fetch-profile-gate` + `decisions/plugin-cookie-jars`. Motivation: proposal.md.

## Goals / Non-Goals

**Goals:** one gate (`src/fetch/`) — single impersonate call site; per-call profile + live default; persistent per-plugin cookie jar; parity with existing http behavior.

**Non-Goals:** firefox target (phase 2, separate .so); a hand-rolled JA3/UA profile class; the read-model cache (change `search-cache`); per-request user-agent overrides.

## Decisions

- **D1 — Ownership moves wholesale.** `FetchResult`, body/header sinks, the impersonate retry, `applyProxy`, and the option-set block move into `fetch::sync`; plugin.cpp only assembles a `Request`. No curl logic remains in plugin.cpp (grep-able: one `curl_easy_impersonate` in the tree).
- **D2 — Profile resolution & live default.** Empty `Request.profile` => default; the config snapshot is read at call time (`std::shared_ptr<const Config>` on the host, swapped atomically by `POST /config`) — closures no longer capture config values. Unknown target => warn + use the default (the request still runs).
- **D3 — Cookie engine.** `Request.cookie_jar` (path): `CURLOPT_COOKIEFILE(path)` + `CURLOPT_COOKIEJAR(path)`; empty = not set (byte-identical legacy behavior). Helper `plugin_state_dir/<id>.cookies.txt` on the plugin side; file failures => warn, request proceeds without persistence.
- **D4 — ABI opts.** `profile` (optional string) and `cookie` (bool/minimal object) on lua `http.fetch` opts, the js opts object, and the wasm import (`env.http_fetch` opts JSON) — absent fields = today. Envelope/response unchanged.
- **D5 — Legacy parity.** The old lua path (:562) folds into the gate; response shape and the ponytail retry are preserved; diff behavior guarded by existing tests (test_plugin 10/10 required).
- **D6 — Testing. Unit: target-name validation + fallback, jar round-trip in a tmp dir, two-jar isolation; parity: existing tests. Live smoke: `PLUGIN_SMOKE_URL` style, optional; ctest stays network-free.

## Risks / Trade-offs

- Touches the hot path of every plugin — mitigated by parity tests + a mechanical move.
- Live default via `POST /config`: the snapshot swap must be thread-safe (atomic shared_ptr) — the pattern already exists in store conf reads.

## Open Questions

None; the target list follows the OKF probe (chrome99-116/edge99-101/safari15).
