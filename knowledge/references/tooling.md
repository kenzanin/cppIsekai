---
type: reference
title: "Test and lint tooling (doctest, clang-tidy, clang-format)"
description: "doctest + ctest + clang-tidy/format targets wired; system packages only; LIKE-% lesson recorded"
tags: [testing, tooling, lint]
generated: { by: agent/mcp, at: "2026-11-06T13:57:15Z" }
status: stable
---

Test and lint tooling wired, all from system packages (doctest 2.5.3, clang-tidy/clang-format 23.1.4).

- tests/test_store.cpp: doctest suite against Store (seed shape, count no-filter/plugin/LIKE-escaping, pagination ordering, join). One db file per run (test_store.db, gitignored). 18 assertions green.
- CMake: test_store target (store.cpp + tests), enable_testing + add_test(store). cmake --build build then `cd build && ctest`.
- `tidy` target: clang-tidy -p build over src/*.cpp. .clang-tidy: bugprone/cert/clang-analyzer/modernize/performance/portability/readability minus noisy checks (magic-numbers, easily-swappable-parameters, trailing-return-type). HeaderFilterRegex scoped to src|tests.
- `format` target: clang-format -i over src/*.cpp *.hpp tests/*.cpp. .clang-format: LLVM-based, 4-space, 120 cols.
- Lesson: '%' trailing pattern matches #10-#12 ('Scans #1' finds 4 rows) -- write pagination/search tests against name-ordering, not plugin continuity.
- data/ (runtime db+log) untracked after initially slipping into the ORM commit; .gitignore now covers data/, test_store.db, build/, tmp/.

# Related Concepts
- [Source layer map and rules (store / log / http)](code-layers.md): Test/lint tooling executes the code-layer rules for future modules
