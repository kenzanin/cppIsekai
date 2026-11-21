---
type: Decision
title: Golden-file render test for top-level templates
description: Top-level mustache pages are locked by golden-file render tests that catch broken partial includes and dropped context keys.
tags: [testing, templates, mustache, golden, regression, alpine]
generated: { by: agent/mcp, at: "2026-11-21T10:29:11Z" }
status: stable
---

## Decision

Render each route-served top-level template (home, grid, search, reader, detail, blank, plugins, settings, logs) with a representative context in `tests/test_templates.cpp` and diff the output against `tests/fixtures/golden/<name>.html`.

- Loader: `crow::mustache::set_base(<baked templates dir>)` then `load(top).render(ctx).dump()` — partials resolve through the same loader, so the test is cwd-independent (dirs baked via `target_compile_definitions`).
- Regenerate after an intentional markup change: `CPPISEKAI_UPDATE_GOLDEN=1 ./build/test_templates`. Two-step re-lock: the rewrite run stays RED until `CPPISEKAI_UPDATE_GOLDEN_ACK=1` is also set.
- Context keys mirror what each route's context builder actually sets (`home_context`, `layout()`, reader resume keys, logs `lines`), so a renamed/absent key fails.
- `layout.mustache` is excluded: nothing references it (no `{{>body}}` provider), so it is dead.

## Why

The x9 partial-split left ~22 templates wired through partial includes; golden files are the only cheap lock on that graph.

## Template tag balance (2026-11-21)

A golden diff alone cannot catch an **unbalanced Alpine `<template>`**: an orphan closing tag throws `ReferenceError: u is not defined` at component init, and every later binding silently dies (detail page showed "No chapters" and no enrichment sources despite the APIs returning 93 chapters). Regression introduced while moving a SRC cover block out of a per-source loop.

`check_golden` now counts `<template` vs `</template>` in every rendered page and `CHECK(opens == closes)`. Proven to guard: deleting the loop's opening tag makes `test_templates` RED; restoring it makes the suite 20/20 again. Lesson: markup-shape invariants need a structural assertion, not just byte comparison against a stale golden.

Also: static `detail.js` / `reader.js` are served from precompressed siblings — editing the `.js` alone is a live no-op until `xmake js` regenerates the `.br` (minify + brotli q11).
