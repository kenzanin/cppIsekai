# Tasks

## 1. Runtime-agnostic core

- [x] 1.1 Extract envelope build / response-shape validation / health bookkeeping from `Host::call` into a runtime-agnostic core; `Runtime` gains a variant holder; Lua dispatch behavior unchanged; existing `test_plugin` green
- [x] 1.2 `fs_read(plugin_dir, rel)` helper (normalize, confine to plugin dir, regular files only) + `fs.read` bound into the Lua runtime; unit tests: own-file ok, `..` escape, absolute path, missing file

## 2. WASM runtime (wasm3)

- [x] 2.1 `wasm_rt` adapter: module load from entry `.wasm`, `handle/alloc/result_len` ABI per design D3, host-tool imports (`http.fetch`, `storage.*`, `log.*`, `now`, `fs.read`) and fuel/stack limits; failure to load => plugin unregistered + logged
- [x] 2.2 Echo fixture plugin (`tests/fixtures/plugins/wasm/`, prebuilt `.wasm` + regen note) and ctest cases: happy path call, bad envelope, budget loop, fs.read confined

## 3. JS runtime (quickjs-ng)

- [x] 3.1 Vendor quickjs-ng static sources under `third_party/quickjs/` + wire into CMake (`cppIsekai` + test targets), attribution header
- [ ] 3.2 `js_rt` adapter per design D4: realm per plugin, whitelisted globals incl `fs.read`, interrupt budget, `handle(env)` dispatch, exception => ok:false; failure to load => unregistered + logged
- [ ] 3.3 Echo fixture plugin (`tests/fixtures/plugins/js/main.js`) and ctest cases: happy path, syntax-error entry, budget loop, host-tool parity spot-check

## 4. Verify

- [ ] 4.1 Full `ctest` green, zero new warnings; `openspec validate --specs`
- [ ] 4.2 Live smoke: deploy js+wasm fixture plugins to a scratch plugins dir, boot daemon, `curl` the plugin call route for both runtimes (happy + error path), then remove
