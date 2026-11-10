---
type: decision
title: "Source layout: crown facades, ~200-loc units, clang-doc comments"
description: "Change source-split-clangdoc: crown files become thin facades; logic lives in units of ~200 code-loc or less under src/plugin/{lua,wasm,js} and src/http/{http_pages,http_api}; runtime seams per-folder; public doc-comments clang-doc with why-notes as //; config.hpp (185) and cache.cpp (177) stay unsplit sub-200; audit table in the change codeloc-final.txt."
tags: [layout, refactor, clang-doc, code-loc, structure]
generated: { by: agent/reasonix, at: "2026-11-10T14:32:22Z" }
status: stable
---

Decision recorded from change source-split-clangdoc (14/14 tasks; audits in
openspec/changes/source-split-clangdoc/codeloc-final.txt).

## Policy

1. Crown files are thin facades (composition, registration, the shared
   core). Logic lives in units of ~200 code-loc or less, D1b metric
   (comments/blank stripped).
2. Plugin runtime seams live per-runtime: `src/plugin/lua/`, `src/plugin/wasm/`,
   `src/plugin/js/`; shared runtime types in `src/plugin/runtime_common.hpp`;
   the interim sync client in `src/plugin/http_client.{hpp,cpp}` (moves to
   src/fetch/ with change fetch-gate). HTTP registration splits into
   `http_pages.cpp` / `http_api.cpp` behind `http_routes.hpp`.
3. Documented >200 exceptions (large functions stay whole): `plugin.cpp` 453
   (entry/health/load_all) and `http_api.cpp` 305 (route bodies).
4. Doc-comments: `/** \brief */` clang-doc style on public declarations;
   why/ponytail notes stay `//` line comments.
5. Sub-200 files are not split (4.x pruned per task 1.0): `config.hpp` 185,
   `cache.cpp` 177.
6. New modules follow the same caps; the crown list lives in AGENTS.md
   Conventions and must be updated on layout changes.

## 2026-11-10 addendum (module-split follow-through)

Further splits landed on top of this policy, keeping the same caps and
per-folder seams:
- fetch (change fetch-gate): curl family fn-tables moved to
  `fetch/fetch_curl.cpp` behind `fetch_internal.hpp` (chrome link vs
  dlopen'd firefox/system siblings); transfer body stays in `fetch.cpp`.
- htmltext: re2 and lexbor halves split into `htmltext_re2.cpp` /
  `htmltext_html.cpp`; `trim_text` shared via `htmltext_internal.hpp`;
  dead `json_string_array` dropped.
- wasm: `env.*` host imports moved to `wasm_imports.cpp` with
  `link_imports()` seam; guest memory helpers (guest_mem/guest_str/
  guest_write) and import-domain constants in `wasm_internal.hpp`;
  `wasm_load.cpp` keeps lifecycle only.
- Templates: inline `<script>` blocks in detail/reader/search/settings/
  plugins extracted into `*_script.mustache` partials (crow mustache
  resolves partials at render time, parent context).
- Config core stays header-only (`config.hpp` + `registry.hpp`): only ~4
  consumers, seam not clean, header-only is deliberate.
- lua_runtime.cpp (240) and js_bindings.cpp (244, ~202-206 code-loc)
  stay unsplit at the cap.
- System sqlite 3.54 replaces vendored libsql-sqlite 3.47 amalgamation
  (full JSONB: jsonb_extract/jsonb_each); see f81f592. MVCC/embedded
  replicas NOT adopted (that is Turso, out of scope for local-first).

## Related

- references/code-layers -- the layer map; this policy supersedes its
  single-file crown descriptions.
- architecture/manga-viewer-core -- the host this layout serves.
- decisions/plugin-runtimes-wasm-js -- the per-runtime seam folders realized
