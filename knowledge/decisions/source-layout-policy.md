---
type: decision
title: "Source layout: crown facades, ~200-loc units, clang-doc comments"
description: "Change source-split-clangdoc: crown files become thin facades; logic lives in units of ~200 code-loc or less under src/plugin/{lua,wasm,js} and src/http/{http_pages,http_api}; runtime seams per-folder; public doc-comments clang-doc with why-notes as //; config.hpp (185) and cache.cpp (177) stay unsplit sub-200; audit table in the change codeloc-final.txt."
tags: [layout, refactor, clang-doc, code-loc, structure]
generated: { by: agent/mcp, at: "2026-11-08T11:30:29Z" }
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

## Related

- references/code-layers -- the layer map; this policy supersedes its
  single-file crown descriptions.
- architecture/manga-viewer-core -- the host this layout serves.
- decisions/plugin-runtimes-wasm-js -- the per-runtime seam folders realized
  here.
- [Source layer map and rules (src/<scope>/, .mustache templates)](../references/code-layers.md): supersedes the single-file crown descriptions in the layer map
- [cppIsekai core architecture: plugin-per-site manga viewer](../architecture/manga-viewer-core.md): layout policy for the host codebase this core serves
