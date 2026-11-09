# Tasks: plugin-helper

1. Core + parity
   - [x] 1.1 Vendor/locate deps: system `re2` headers (pacman) or vendor `third_party/re2`; vendor `third_party/lexbor`; wire CMake (lib + include paths)
   - [x] 1.2 `src/plugin/htmltext.{hpp,cpp}`: RE2 match/find + lexbor select/attr (JSON-able vectors, throw-on-bad-pattern contract per D4) + `tests/test_htmltext.cpp`
   - [x] 1.3 `host.*` bindings all three runtimes (lua sol2 table, js frozen global, wasm `env.host_<group>_<fn>` imports per D5) incl. lua `fs.read` parity
   - [x] 1.4 Migrate `plugins_dev/lua/mangadex/main.lua` to `host.*`; grep plugins_dev for stragglers
2. Verification
   - [x] 2.1 Parity doctest per D6: 11-tool set resolves in lua/js/wasm fixtures; identical JSON shapes; RE2/DOM scenario cases from spec
   - [x] 2.2 Full ctest green (zero new warnings) + `openspec validate --specs`; live smoke: mangadex search/detail unchanged through the migrated plugin
