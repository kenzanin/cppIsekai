---
type: Decision
title: xmake build system alongside cmake + prod-code gate tasks
description: "xmake.lua added as primary build path (cmake kept, ./build deleted and rebuilt by xmake); gate tasks format/tidy/fix/gate scoped to prod src only; 16/16 tests pass; xmake-skills plugin installed globally."
tags: [build, xmake, cmake, clang-tidy, clang-format, gate]
generated: { by: agent/mcp, at: "2026-11-12T04:08:08Z" }
status: stable
---

Decision (2026-11-12): xmake added as the primary build path; CMakeLists.txt stays for reference but the cmake ./build dir was deleted and rebuilt by xmake (build/ now holds only the xmake layout: build/linux/x86_64/release/<target>).

Files: xmake.lua (new), .gitignore (+compile_commands.json), run-cppIsekai.sh (now points at build/linux/x86_64/release/cppIsekai).

Target parity with CMakeLists.txt: vendored static C libs (wasm3, quickjs, lexbor), daemon cppIsekai (src/**.cpp + libzippp), 16 doctest binaries registered with add_tests (run: `xmake test` — 16/16 pass, parity with ctest).

Gate tasks (prod code src/** only, tests/third_party untouched — user requirement): `xmake format` (clang-format -i), `xmake tidy` (clang-tidy report), `xmake fix` (clang-tidy -fix -fix-errors), `xmake gate` (format+fix+build+test). First format run rewrote 55 prod files (+1204/−893): the codebase had drifted from .clang-format (120 cols).

Gotchas hit:
1. import() is script-domain only (on_load/on_run) — not available at xmake.lua root scope. mold detection lives in on_load.
2. pkg-config deps go through add_requires("pkgconfig::X", {system=true}) + add_packages — there is no add_pkgconfig in xmake 3.1.2.
3. add_files needs flat string lists (no nested tables).
4. set_languages("c23") is strict ISO: hides POSIX symbols (strnlen in wasm3) that cmake's default gnu23 exposed — vendor C libs need _GNU_SOURCE.
5. Task on_run executes in a fresh Lua sandbox: globals defined in xmake.lua are NOT visible; os.exec does NOT emulate shell pipes (passes | as argv). Use os.files + os.execv inline in on_run.
6. Tasks need set_menu to be invocable as `xmake <name>` (see xmake-custom-plugins skill).
7. xmake task list: xmake-skills plugin package installed globally from github.com/xmake-io/xmake-skills (58 skills, commit ef67caa).

Toolchain: xmake auto-uses ccache; mold via on_load ldflags. clang-tidy volume is huge on heavy template TUs (~25k generated/file, suppressed by HeaderFilterRegex to src/*.hpp) — WarningsAsErrors stays empty so the gate reports but never blocks.
