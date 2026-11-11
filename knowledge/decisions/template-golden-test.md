---
type: Decision
title: Golden-file render test for top-level templates
description: Top-level mustache pages are locked by golden-file render tests that catch broken partial includes and dropped context keys.
tags: [testing, templates, mustache, golden, regression]
generated: { by: agent/mcp, at: "2026-11-11T07:19:12Z" }
status: stable
---

## Decision

Render each route-served top-level template (home, grid, search, reader, detail, blank, plugins, settings, logs) with a representative context in `tests/test_templates.cpp` and diff the output against `tests/fixtures/golden/<name>.html`.

- Loader: `crow::mustache::set_base(<baked templates dir>)` then `load(top).render(ctx).dump()` — partials resolve through the same loader, so the test is cwd-independent (dirs baked via `target_compile_definitions`).
- Regenerate after an intentional markup change: `CPPISEKAI_UPDATE_GOLDEN=1 ./build/test_templates`.
- Context keys mirror what each route's context builder actually sets (`home_context`, `layout()`, reader resume keys, logs `lines`), so a renamed/absent key fails.
- `layout.mustache` is excluded: nothing references it (no `{{>body}}` provider), so it is dead.

## Why

The x9 partial-split left ~22 templates wired through `{{>...}}` includes. A typo'd include name or a dropped context key renders silently wrong at runtime. Mutation-verified: renaming `grid_body.mustache` and renaming `{{total}}` each fail the golden (exit 1, first-mismatch line reported).

## How to apply

Run `ctest -R templates` in the full gate. When a template's markup changes on purpose, regenerate the goldens in the same commit as the template edit.
