# Tasks

## 1. plugin module split

- [ ] 1.1 Extract the lua layer: `plugin_lua.cpp` + internal `lua_bridge.hpp` (bridges, sandbox, host tools, budget hook, `lua_call_entry`); split `lua_bridge.cpp` out too if the unit lands >300 (pre-approved, design D2)
- [ ] 1.2 Extract the sync http client: `http_client.cpp` + internal `http_client.hpp` (`FetchResult`, sinks, `fetch_sync`, `applyProxy`, `make_wasm_fetch`); interim home until `fetch-gate`
- [ ] 1.3 Slim `plugin.cpp` to the Host core; wire CMake source lists; full ctest green

## 2. http module split

- [ ] 2.1 Registration-helper extraction per design D3: `http_pages.cpp` + `http_api.cpp` + internal `http_routes.hpp`; `serve()` slims to layout/helpers + registration calls; full ctest green

## 3. runtime seams

- [ ] 3.1 `wasm_rt.cpp` -> `wasm_rt.cpp` (invoke/guest_mem) + `wasm_load.cpp` (dtor/load/imports); ctest green
- [ ] 3.2 `js_rt.cpp` -> `js_rt.cpp` (load/invoke/lifecycle) + `js_bindings.cpp` (C callbacks + bind helper); ctest green

## 4. config & cache

- [ ] 4.1 `config.hpp` -> umbrella + `config_registry.hpp` + `config_io.hpp`; consumer surface unchanged (`config.hpp` still the include); ctest green
- [ ] 4.2 `cache.cpp` -> LRU core + `cache_scan.cpp` (boot walk, purge-once); ctest green

## 5. clang-doc conversion

- [ ] 5.1 Headers: `/** \file */` banners on new units; `\brief`/`\param`/`\return` on every function declaration in `src/**.hpp`
- [ ] 5.2 Sources: convert pre-existing doc-comments on definitions; keep ponytail/why notes as plain comments

## 6. Verify & bookkeeping

- [ ] 6.1 Full ctest green, zero new warnings; `wc -l` audit vs design D2; `git diff --color-moved=plain` spot check; optional `clang-doc` dry-run
- [ ] 6.2 Update the AGENTS.md crown-file note; record the layout decision in OKF (close pipeline)
