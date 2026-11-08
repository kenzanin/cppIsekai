# Proposal

## Why

A loc scan of production sources (`find src -name '*.cpp' -o -name '*.hpp' | xargs wc -l`, 2026-11-08) shows six files above 200 lines:

| file | loc |
|---|---|
| `src/plugin/plugin.cpp` | 1003 |
| `src/http/http.cpp` | 564 |
| `src/plugin/wasm_rt.cpp` | 355 |
| `src/plugin/js_rt.cpp` | 259 |
| `src/config/config.hpp` | 238 |
| `src/cache/cache.cpp` | 220 |

Monolithic crowns slow review, hide module seams, and make the pending feature changes
(`fetch-gate`, `search-cache`) collide. User directive: split by moving code only, and
move documentation comments to clang-doc (Doxygen) format.

## What Changes

- **Mechanical splits (move-only)** of the six crowns into seam-aligned units (see design D2 for the map); no logic, ordering, or behavior changes.
- **clang-doc conversion**: function documentation in `src/**` becomes `/** \brief ... \param ... \return ... */`; rationale/ponytail notes stay as plain comments (design D4).
- **No dependency, build-behavior, or ABI changes**; CMake source lists gain the new translation units; internal headers stay module-private (`src/<mod>/`).
- Out of scope: renaming public APIs, changing include graph for consumers (umbrella `config.hpp` keeps its role), feature work.

## Capabilities

### New Capabilities

(none — pure refactor; `skip_specs: true` set in `.openspec.yaml`)

### Modified Capabilities

(none)

## Impact

- Files: the six crowns slimmed; new units under `src/plugin/`, `src/http/`, `src/config/`, `src/cache/` (per design D2); CMakeLists source lists; `AGENTS.md` crown-file note.
- Sequencing: recommended AFTER `fetch-gate` and `search-cache` land (both rewrite `plugin.cpp`/`http.cpp` heavily). If applied earlier, those changes' task file references need a one-hop update — cheap, mechanical.
- Risk: low (behavior-preserving moves); guarded by the full suite, zero-new-warning gate, and a move-diff audit (`git diff --color-moved=plain`).
