# Proposal: plugin-helper

## Why

All plugin runtimes (lua, js, wasm) must expose the **same host tools with the same names and signatures**. Today they are 90% parity — the only gap is `fs.read` (js/wasm have it, lua does not). Beyond closing that gap, plugins that scrape heavy HTML sites cannot do parsing inside the script world (100x slower than native); the host must provide the expensive primitives and hand back structured results. External reference (`tmp/plugin_interface.md`) calls these helper functions: DOM selection by CSS selector and regex extraction, backed by linear-time engines (lexbor for DOM, RE2 for regex) so hostile input cannot stall the host.

This change: (1) closes the `fs.read` parity gap, (2) adds `re2.match`/`re2.find` host helpers (RE2), (3) adds `html.select`/`html.attr` host helpers (lexbor), all exposed identically to lua/js/wasm and passing the shared response-shape gate.

## What Changes

- **NEW** `src/plugin/htmltext.hpp/.cpp` — host-side primitives: RE2-backed `re2.match(re, text)` / `re2.find(re, text)`; lexbor-backed `html.select(html, css)` / `html.attr(html, css, attr)`. Both return JSON-able arrays; errors return empty results, never throw.
- **NEW bindings in all three runtimes**: `re2` and `html` namespaces exposed to lua (sol2), js (quickjs globals), wasm (env.* imports) — same names, same arg order, same return shapes as each other.
- **FIX + MIGRATE** — unified `host.*` namespace (user steer 2026-11-09): ALL host tools move under `host.` in all three runtimes (`host.http.fetch`, `host.storage.get/set`, `host.log.info/error`, `host.now`, `host.fs.read`) — closing the lua `fs.read` gap at the same time — plus the new `host.re2.match/find` and `host.html.select/attr`. Old bare globals (`http`, `storage`, `log`, `now`) are removed, not aliased: plugins are in-house, the one existing plugin (mangadex) is migrated in this change.
- **Vendor** `third_party/re2` + `third_party/lexbor` (system lib if present via pacman, else vendored source — per `decisions/deps-and-plugins`).
- **Tests** `tests/test_htmltext.cpp`: RE2 and DOM behavior; one parity case per runtime proving lua/js/wasm all see identical names/signatures/returns.

## Capabilities

### plugin-system (MODIFIED)
- Adds `fs.read` to the lua host-tool surface (parity requirement satisfied for all runtimes).
- Adds `re2.*` and `html.*` to the host-tool surface, required to be identical across runtimes.

## Impact

- **code**: `src/plugin/` (+new `htmltext.*`, bindings in 3 runtimes, lua fs.read), `CMakeLists.txt` (2 libs + 1 test target), `third_party/` (+re2, +lexbor), `tests/`.
- **specs**: `openspec/specs/plugin-system/spec.md` gains the helper-parity requirement at archive-sync.
- **risk**: two new vendored deps (RE2 ~large but header+cc self-contained; lexbor C, small). Regex patterns come from plugin authors — RE2 is the safe choice (linear time, no ReDoS).
- **compat**: purely additive to the plugin ABI; existing plugins untouched.
