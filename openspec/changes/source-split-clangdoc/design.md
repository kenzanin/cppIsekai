# Design

## Context

Scan results and the two pending changes are in proposal.md. Constraints from AGENTS.md: crown files convention (updated by this change), header-only preference for leaf modules, zero-new-warning gate, English-only artifacts. User constraints: split by MOVE only; clang-doc comment format.

## Goals / Non-Goals

**Goals:** every production file at a natural seam and roughly <=250 loc (documented exceptions up to ~300 for plugin core / http api); zero behavior delta; clang-doc on all function documentation.

**Non-Goals:** renames, public-API changes, include-graph changes for consumers, cache/store schema work, comment prose rewrites beyond the conversion rule.

## Decisions

- **D0 — Sequencing.** Land after `fetch-gate` + `search-cache`. Their tasks reference `src/plugin/plugin.cpp` / `src/http/http.cpp`; post-split those become the slimmed cores + new units — a one-hop mapping done during their apply.
- **D1 — Move-only discipline.** Functions/types move verbatim; internal linkage stays via per-file anonymous namespaces; new internal headers (`src/plugin/lua_bridge.hpp`, `src/plugin/http_client.hpp`, `src/http/http_routes.hpp`) are module-private; includes re-sorted; no signature changes.
- **D2 — Split map (est. loc after):**
  - `plugin.cpp` 1003 -> `plugin.cpp` (Host core: Impl/registry/load/call/health/logo, ~300) + `plugin_lua.cpp` + `lua_bridge.hpp` (sol/js bridges, sandbox, host tools, budget hook, `lua_call_entry`, ~420 -> see 1.1 note: bridges `json_to_sol`/`sol_to_json` stay with the lua unit; if it lands >300, split `lua_bridge.cpp` further at apply) + `http_client.cpp` (FetchResult/sinks/`fetch_sync`/`applyProxy`/`make_wasm_fetch`, ~180; interim home until `fetch-gate` relocates it to `src/fetch/`).
  - `http.cpp` 564 -> `http.cpp` (`serve`, layout/html/error helpers, ~180) + `http_pages.cpp` (page routes via registration helpers, ~140) + `http_api.cpp` (JSON routes: plugins list/call, reader, skip, config POST, cache/img, export/cbz, logs, ~250).
  - `wasm_rt.cpp` 355 -> `wasm_rt.cpp` (`invoke`, `guest_mem`, ~140) + `wasm_load.cpp` (Instance dtor, `load`, import table/link, ~215).
  - `js_rt.cpp` 259 -> `js_rt.cpp` (`load`/`invoke`/lifecycle, ~110) + `js_bindings.cpp` (all `JSValue` C callbacks + bind helper, ~150).
  - `config.hpp` 238 -> umbrella `config.hpp` (types + includes, unchanged consumer surface) + `config_registry.hpp` (knob table, ~120) + `config_io.hpp` (SimpleIni load/save/dump, ~110).
  - `cache.cpp` 220 -> `cache.cpp` (LRU core, ~130) + `cache_scan.cpp` (boot walk, purge-once, ~90).
- **D3 — http route extraction pattern.** Route lambdas move into `register_pages(...)` / `register_api(...)` free functions taking the same captured references (`store`, `plugins`, `images`, `db`, pools) by signature — registration order preserved; crow route macros unchanged; behavior identical.
- **D4 — clang-doc policy.** Every function declaration in `src/**.hpp` (public and internal) gets `/** \brief ... */` with `\param`/`\return` where non-trivial; new units get `/** \file */` banners. In cpp files, only pre-existing doc-comments on definitions are converted; rationale/ponytail/why notes remain plain `//` (they are not API docs). No prose rewrites.
- **D5 — Verification.** Per-task: focused ctest; final: full ctest 10/10, zero new warnings, `git diff --color-moved=plain` spot-audit (moved lines recognized), `wc -l` audit vs the D2 table, `clang-doc` dry-run over `src/` if the binary exists (best-effort).
- **D6 — Convention bookkeeping.** `AGENTS.md` crown-file note updated to the new layout; completion records the layout in OKF per the close pipeline.

## Risks / Trade-offs

- Extraction of route lambdas into registration functions is the least mechanical step — mitigated by signature-identical captures and the existing suite.
- `plugin_lua.cpp` may land >300; pre-approved fallback: split bridges into `lua_bridge.cpp` at apply time (same pattern, noted in tasks).

## Open Questions

None.
