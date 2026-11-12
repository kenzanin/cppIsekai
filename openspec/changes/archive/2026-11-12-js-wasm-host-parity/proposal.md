# Proposal

## Why

Porting the five goIsekai plugins (`batcave`, `weebcentral`, `comik` → Lua; `komix.to`, `mangapill` → WASM) requires the JS and WASM runtimes to offer the same host surface the Lua runtime already ships — the mangafire port hit the analogous wall and had to grow `host.crypto.vrf_sign` first. Audit of `src/plugin/{lua,js,wasm}` against the helpers the five candidates actually call shows four real gaps and one false alarm.

## What Changes

- **`host.url_encode` / `host.url_decode` added to JS and WASM.** Every candidate calls url encoding; Lua has it, the other runtimes do not. JS: percent-encode with `encodeURIComponent` semantics via host-side C++ (single implementation, no new dependency). WASM: flat imports `url_encode`/`url_decode` with the same C++ core.
- **`host.config.get(key)` added to JS.** Lua has `config.get` (plugin folder `config.json`, host-parsed, missing key → nil). JS gets the same contract. WASM is explicitly out of scope for config (no candidate reads it; noted in design.md).
- **`host.crypto.vrf_sign(api_path, params, stages)` added to JS.** Reuses the shared C++ VRF core (`src/plugin/vrf.hpp`) via the same seam mangafire uses — parity so a future JS port of mangafire does not need a new change. WASM out of scope (no candidate needs it).
- **`host.html.find_list_text` / `find_list_attr` are NOT added.** `host.html.select`/`host.html.attr` already return arrays of all matched elements (trimmed text / attr strings) in all three runtimes — they ARE the list selectors, under different names. Design.md records the mapping `find_list_text → select`, `find_list_attr → attr`; ports use the existing tools. No host code for this item.
- **`http.fetch` opts parity: audit, no expected changes.** JS and WASM already share one `http_fetch` implementation (`make_wasm_fetch`, `src/plugin/plugin_load.cpp`) with the Lua path, parsing `timeout_ms`, `profile`, `method`, `body`, `accept`, `user_agent`, `headers`, `cookie`. The audit verifies this and the JS error/undefined mapping against the spec; fix only if the audit finds a real divergence.
- **Unit tests** for the three new host tools (JS binding smoke + WASM import parity, extending `tests/test_plugin.cpp` with the existing jsfix/wasm fixtures), zero-warning build, `xmake test` green.

## Capabilities

### New Capabilities

(none)

### Modified Capabilities

- `plugin-host-tools`: JS and WASM bindings gain `url_encode`/`url_decode`; JS binding gains `config.get` and `crypto.vrf_sign`; the spec's runtime-parity language is tightened to name the three-way set explicitly, and the list-selector decision (`host.html.select`/`attr` doubles as the find_list equivalent) is recorded.

## Impact

- `src/plugin/js/js_bindings.cpp` — three new bindings (url encode/decode via shared C++ core, config.get via `plugin::fs_read` + glaze parse, vrf_sign via `vrf.hpp`).
- `src/plugin/wasm/wasm_imports.cpp` + `src/plugin/wasm/wasm_internal.hpp` — flat imports `url_encode`/`url_decode` (reuse the same C++ core; no JSON/string-building helpers added otherwise).
- Shared C++ core for URL codec (likely `src/plugin/url_codec.hpp` header-only, no new dependency).
- `tests/test_plugin.cpp` — JS/WASM binding tests (url round-trip, config.get missing-key, vrf_sign vector parity with lua path).
- `openspec/specs/plugin-host-tools/spec.md` — delta spec for the above.
- No changes to fetch stack, store, config, or call sites; no new dependencies.
