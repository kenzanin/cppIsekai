# Tasks

## 1. plugin module split

- [x] 1.0 Baseline re-measure on the D1b code-loc metric (comments/blank stripped) across `src/**`; publish the table and prune the split scope to files actually >200 code-loc (borderlines: cache, js_rt, config may drop out)
- [x] 1.1 Hoist shared runtime types: `RawReply` + `Services` from `wasm_rt.hpp` -> new `src/plugin/runtime_common.hpp`; `plugin.cpp`, wasm, js include it
- [ ] 1.2 Extract the lua layer under `src/plugin/lua/`: `lua_runtime.cpp` + internal `bridge.{hpp,cpp}` (bridges, sandbox, host tools, budget hook, `lua_call_entry`); split `bridge.cpp` further if the unit lands >300 (pre-approved, design D2)
- [ ] 1.3 Extract the sync http client: `src/plugin/http_client.{hpp,cpp}` (`FetchResult`, sinks, `fetch_sync`, `applyProxy`, `make_wasm_fetch`); root-level, interim home until `fetch-gate`
- [ ] 1.4 Slim `plugin.cpp` to the Host core (stays at `src/plugin/` root); wire CMake source lists; full ctest green

## 2. http module split

- [ ] 2.1 Registration-helper extraction per design D3: `http_pages.cpp` + `http_api.cpp` + internal `http_routes.hpp`; `serve()` slims to layout/helpers + registration calls; full ctest green

## 3. runtime seams

- [ ] 3.1 wasm -> `src/plugin/wasm/`: `wasm_rt.{hpp,cpp}` (invoke/guest_mem) + `wasm_load.cpp` (dtor/load/imports); ctest green
- [ ] 3.2 js -> `src/plugin/js/`: `js_rt.{hpp,cpp}` (load/invoke/lifecycle) + `js_bindings.cpp` (C callbacks + bind helper); ctest green

## 4. config & cache

- [ ] 4.1 `config.hpp` -> umbrella + `config_registry.hpp` + `config_io.hpp`; consumer surface unchanged (`config.hpp` still the include); ctest green
- [ ] 4.2 `cache.cpp` -> LRU core + `cache_scan.cpp` (boot walk, purge-once); ctest green

## 5. clang-doc conversion

- [ ] 5.1 Headers: `/** \file */` banners on new units; `\brief`/`\param`/`\return` on every function declaration in `src/**.hpp`
- [ ] 5.2 Sources: convert pre-existing doc-comments on definitions; keep ponytail/why notes as plain comments

## 6. Verify & bookkeeping

- [ ] 6.1 Full ctest green, zero new warnings; code-loc audit per D1b (comments/blank stripped) vs the pruned scope; `git diff --color-moved=plain` spot check; large functions verified unsplit; optional `clang-doc` dry-run
- [ ] 6.2 Update the AGENTS.md crown-file note; record the layout decision in OKF (close pipeline)
