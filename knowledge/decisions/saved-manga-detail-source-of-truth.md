---
type: decision
title: "Saved manga detail lives on saved_manga (planned, openspec manga-detail-table)"
description: "IMPLEMENTED (c46ca53, merge 651bdc7, archived 2026-11-15): a saved manga's display metadata lives on saved_manga itself (name, cover, status + authors, artists, tags, synopsis_display); route-level materialize_detail writes resolved values, apply_chosen stays pure, /api/detail serves the row cached:true with the enrichment overlay, reset detail/all clears via saved_detail_clear, library upsert guards the name."
tags: [detail, saved_manga, store, openspec, source-of-truth, landed, regression-note]
generated: { by: agent/mcp, at: "2026-11-15T05:42:18Z" }
status: stable
---

## Decision

A saved manga's display detail lives on `saved_manga` itself: `name`, `cover`, `status` plus `authors`, `artists`, `tags`, `synopsis_display` (additive `TEXT DEFAULT ''` migration, entity + migration in one commit). No new `manga_detail` table. Plugin-source `synopsis`/`alt_titles` stay untouched because duplicate clustering reads them.

## Write path

Route-level `materialize_detail(...)` (src/http/enrich_internal.hpp) writes the resolved values, called from `/api/detail` and `refresh_title` (detail method only) - never from `apply_chosen`, which stays a pure merge (enrichment guard: candidates never touch `saved_manga`). Skipped when unchanged. The tags written are the post-merge list the page renders.

## Read path

`/api/detail` answers a saved manga from its row, flagged `cached:true`, still carrying `enrichment` + `chosen_cover`. `GET /api/library` emits the four columns only when non-empty (sparse). Chapters keep the live/stale list path - the row-serve branch is `detail`-endpoint only (regression 86696e2: applying it to `/api/chapters` returned an object where the page expects a list, `slice is not a function`).

## Gotcha (cost a debug cycle)

Adding entity columns is not enough: `Store::saved_get` AND the `saved_list` projection must both copy the new fields, else reads silently return empties while writes succeed.

## Freshness (accepted limitation)

Metadata refreshes on explicit refresh (rocket Update) or cold miss; auto-update stays `chapters_only`. Upgrade path: drop `chapters_only` from the auto-update pass or add a config knob.

## Landed

Commits c46ca53 (feature) + 86696e2 (chapters regression) on merge 651bdc7; change archived to `openspec/changes/archive/2026-11-15-manga-detail-table/`; specs `manga-detail-page`, `library-save`, `detail-enrichment-fields` updated.
