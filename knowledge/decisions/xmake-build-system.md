---
type: Decision
title: xmake build system alongside cmake + prod-code gate tasks
description: "xmake is the primary build path (cmake kept, ./build rebuilt by xmake). Daily: xmake -j → xmake test; gate = format/fix/build/test with tidy/fix diff-scoped vs HEAD since 2026-11-12 (5a41afc) — vendor-template TUs skip the crawl; always build+test after fix, its reflow has eaten separators. Original decisions d019d91/3a8a466/12b7202. 2026-11-19: C++23 modules probe P0 killed -- 18-28x slower on every metric (scan-deps per TU per build); headers stay, revisit ~10x TU."
tags: [build, xmake, cmake, clang-tidy, clang-format, gate, workflow, diff-scope]
generated: { by: agent/mcp, at: "2026-11-19T03:38:52Z" }
status: stable
---

Decision (2026-11-12, committed d019d91/3a8a466/12b7202): xmake primary build path; CMakeLists.txt stays reference but cmake ./build dir was deleted rebuilt by xmake (build/ now holds only xmake layout: build/linux/x86_64/release/<target>).

Files: xmake.lua (new), .gitignore (+.xmake/ +compile_commands.json), run-cppIsekai.sh (points at build/linux/x86_64/release/cppIsekai).

## New daily flow
- Build all (daemon + 16 tests): `xmake -j` (ccache automatic, mold detected in on_load).
- Test: `xmake test` (16/16, parity ctest).
- Done gate before declaring work finished: `xmake gate` format (clang-format -i) → fix (clang-tidy -fix -fix-errors) → build → test. Individual: `xmake format` / `xmake tidy` (report only) / `xmake fix`.
- Gate scope: prod code only (src/**); tests/ third_party/ are never auto-edited.
- Run daemon: `./run-cppIsekai.sh` nohup binary `--config data/cppisekai.ini`; restart rebuilding before judging template bugs (templates render disk, endpoints come binary).
- compile_commands.json project root clangd/clang-tidy: `xmake project -k compile_commands` (also run by tidy/fix tasks).
- clang-tidy volume huge on heavy template TUs (~25k generated/file, suppressed HeaderFilterRegex src/*.hpp); WarningsAsErrors stays empty tidy reports but never blocks. Review `xmake fix` diffs deliberately — modernize-* rewrites en masse.

Target parity: vendored static C `git check-ignore -v` after editing.
8. xmake-skills plugin package installed globally github.com/xmake-io/xmake-skills (58 skills, commit ef67caa) — reference xmake idioms.

## 2026-11-12 update: gate tidy/fix diff-scoped; fix-phase separator corruption (commit 5a41afc)

- User ruling "skip vendor in tidy": `xmake tidy` / `xmake fix` now sweep only sources changed vs HEAD plus untracked (ponytail note in xmake.lua). full-tree crawl burned ~20min re-instantiating sol2/quickjs/wasm3/sqlite_orm templates inside untouched TUs. Widen by hand one-off tree-wide pass.
- Gotcha 9: fix phase eats separators while reflowing — `{}, pctx` became `{} pctx` (src/reader/worker.hpp) `,` vanished between defaulted params (src/http/img.hpp); broke build AFTER fix ran. Always build+test immediately `xmake fix` its diff (both repaired in 5a41afc).

## 2026-11-13 update: migration complete — xmake sole builder, 9 deps on xrepo

- CMakeLists.txt retired to tmp/ (untracked); no cmake path remains. `xmake -y` builds, `xmake test` 17/17; zero-warning gate verified on full rebuild (sole residual line: benign "Ascend SDK not found" probe note).
- 9 vendored deps → xrepo pins, locked in xmake-requires.lock (committed): glaze 7.9.0, sol2 3.5.0, thread-pool 5.1.0, readerwriterqueue 1.0.7, lexbor 3.0.0, libzippp 7.1, quickjs-ng 0.15.1, simpleini 4.27, sqlite_orm 1.9.1. third_party/ now holds only wasm3 + curl-impersonate (no xrepo equivalent).
- Gotcha 10: xrepo libzippp nests its header `libzippp/libzippp.h` (export_cbz.cpp include adapted). quickjs-ng 0.15.1 `JS_SetInterruptHandler` takes the runtime, not ctx (js_rt.cpp adapted).
- Targetdir: default xmake layout; run-cppIsekai.sh path unchanged. A transient `outdir=build` override was reverted to keep the launcher script + AGENTS run-line canonical.
- Zero-warning sweep: NSDMI added — PageCtx::chapter_id / CoverCtx::plugin (cache.hpp), WindowCtx::chapter_id (worker.hpp), SavedManga synopsis/alt_titles (store.hpp); vestigial params commented in htmltext_html ddoc + http_pages layout().

## 2026-11-19 update: C++23 modules probe P0 — killed, 18-28x slower

- Probe: `build.c++.modules` policy on every `cxx_bin` target + one named module `cache.slug` (`src/cache/slug.mpp`), 10 includers flipped to `import cache.slug;` (incl. 2 plain headers). clang 23.1.5, libstdc++ 16, 44 TUs.
- Result vs header-include baseline: full rebuild 13.24 s -> 373.6 s (28x), daemon `-r` 152 s, touch `api_ctx` 3.58 -> 98.1 s (27x), touch slug 2.65 -> 47.9 s (18x). Suite 19/19 after revert; tree restored to HEAD.
- Cause: xmake runs the clang-scan-deps module-discovery pass over every TU on every build (heavy crow/sol2/glaze include closure), and the policy must be on EVERY consuming target (daemon + 19 test bins) -- the scan cost is structural, not per-target tunable. Same family as the PCH verdict (refactor-hygiene design D6): fixed instrument cost > work saved at this scale.
- Ruling: header-#include stays. Revisit only at ~10x TU count (~400) or when xmake caches the module-deps scan across builds. Retry gotchas: the `.mpp` must be listed in `files` of every target that imports it (else `missing cache.slug dependency`); `import` inside plain headers works; the edit tool's syntax validator rejects import-line edits -- use sed.

# Related Concepts
- [Build system: CMake + C++23, exe-only](build-toolchain-cpp23.md): C++23 toolchain choice for the build.
- [xmake pkg-install interrupted leaves empty hash-dirs](../constraints/xmake-install-interrupt.md): xmake package install interruption pitfall.
- [browser smoke tooling: blueprint-mcp free mode](browser-smoke-tooling.md): Smoke-test tooling used alongside the build.
