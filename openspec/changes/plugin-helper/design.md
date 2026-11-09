# Design: plugin-helper

## Context

Host tools today are bare globals with 90% cross-runtime parity (lua/js: `http`, `storage`, `log`, `now`; wasm: flat `env.*` imports). Lua lacks `fs.read`. User steer: unify everything under a `host.*` prefix so every host-provided function is visibly namespaced and the parity surface is one flat table.

## Goals / Non-Goals

- Goals: one `host.*` namespace across lua/js/wasm; identical names/signatures/semantics per tool; RE2 + lexbor heavy primitives host-side; existing plugins migrated.
- Non-Goals: backward-compat aliases for old globals (in-house plugins, one site); querySelectorAll advanced pseudo-selectors beyond lexbor's CSS support; incremental DOM streaming.

## Decisions

### D1 — `host.*` namespace, full migration, no aliases (user steer 2026-11-09)
- lua: a single `host` table passed to plugins (`host.http.fetch(...)`, `host.re2.match(...)`); bare `http`/`storage`/`log`/`now`/`fs` globals removed.
- js: single frozen `host` global with the same subtree shape.
- wasm: module imports stay flat (`env.` namespace has no nesting); naming convention `host_<group>_<fn>` (e.g. `env.host_re2_match`) — the parity table documents the wasm mapping.
- Migration: `plugins_dev/lua/mangadex/main.lua` updated in this change (mechanical s/http\./host.http./ etc.). A parity test asserts the same tool names exist in all three runtimes.

### D2 — RE2 for regex (vendored or system)
- `host.re2.match(re, text)` -> array of capture groups (whole match + submatches; empty array when no match). `host.re2.find(re, text)` -> first match array or empty.
- RE2 chosen over std::regex: linear time, no ReDoS from plugin-authored patterns. Invalid pattern raises a script-level error (lua error / JS exception / wasm trap) — loud, debuggable; no-match is NOT an error (empty array), matching the external reference's "return empty, don't throw" rule for data paths.
- System package first (`pacman -S re2` headers exist on Arch); vendor fallback only if linkage fails.

### D3 — lexbor for DOM (vendored)
- `host.html.select(html, css)` -> array of matched elements' text content (trimmed). `host.html.attr(html, css, attr)` -> array of attribute values (missing attr => element skipped).
- lexbor: small C library, vendorable under `third_party/lexbor`; no C++ wrapper needed (thin RAII holder in htmltext.cpp).
- Malformed HTML: lexbor is lenient (browser-like recovery); selector with no matches => empty array, never throw.

### D4 — one host-side core (`src/plugin/htmltext.{hpp,cpp}`)
- Pure C++ functions returning JSON-able `std::vector<std::string>`; no runtime types leak in. Each runtime binding is a thin marshal (sol2 / JS_NewCFunction / m3 raw fn over the shared memory ABI like existing imports).
- Errors (bad regex, internal): throw into the calling script; empty-result paths return empty arrays.

### D5 — wasm ABI for the new imports
- Strings cross via the existing linear-memory convention used by `env.*` imports (ptr/len in, host-allocated result via the established alloc/last_len pattern). New imports: `host_re2_match`, `host_re2_find`, `host_html_select`, `host_html_attr`, `host_fs_read` (if not already present) — results are JSON arrays serialized into the reply buffer.

### D6 — parity test (the point of the change)
- One doctest case per runtime asserting: `host` exists; the exact tool set {http.fetch, storage.get, storage.set, log.info, log.error, now, fs.read, re2.match, re2.find, html.select, html.attr} resolves; a tiny sample call returns the same JSON shape for lua/js/wasm fixtures.

## Risks / Trade-offs

- Removing bare globals breaks any plugin not migrated in the same change — accepted: plugins are in-house; grep plugins_dev during apply to catch stragglers.
- RE2 adds a heavy static lib (~1MB) — acceptable; linear-time guarantee is the reason it exists.
- lexbor CSS selector support is full CSS3 subset — sufficient for scraper selectors; ponytail: no XPath (add later only if a target site demands it).

## Migration Plan

1. Land core + bindings with `host.*` (old globals removed in the same commit set).
2. Migrate mangadex plugin (mechanical rename).
3. Full ctest + live smoke: existing search/detail flow must behave identically.

## Open Questions

- None; wasm import naming (`host_<group>_<fn>`) is a convention, revisit only if wasm plugins complain.
