---
type: decision
title: "Saved manga detail lives on saved_manga (planned, openspec manga-detail-table)"
description: "Planned source of truth for a saved manga's display metadata: the saved_manga row (name, cover, status + new authors, artists, tags, synopsis_display), not a new manga_detail table. Route-level materialize_detail writes resolved values; apply_chosen stays pure. /api/detail serves the row flagged cached with the enrichment overlay; metadata refresh is explicit (rocket Update) since auto-update stays chapters_only. Audit amendments: reset clears materialized fields, library upsert name guard, disabled plugin still serves, migration+entity in one commit, one-name guarantee scoped to library+detail."
tags: [detail, saved_manga, store, openspec, source-of-truth, planned, audit]
generated: { by: agent/mcp, at: "2026-11-15T03:00:52Z" }
status: stable
---

## Decision

A saved manga's display detail lives on `saved_manga` itself: `name`, `cover`, `status` plus new `authors`, `artists`, `tags`, `synopsis_display`. No new `manga_detail` table. Plugin-source `synopsis`/`alt_titles` stay untouched because duplicate clustering reads them.

## Write path

A route-level `materialize_detail(...)` writes the resolved values, called from `/api/detail` and from `refresh_title` (detail method only) - never from `apply_chosen`, which stays a pure merge so the enrichment guard test (candidates never touch `saved_manga`) stays valid. Skipped when unchanged.

## Read path

`/api/detail` answers a saved manga from its row, flagged `cached:true`, and still carries the `enrichment` block + `chosen_cover` so the picker hydrates. Unsaved titles stay live/stale; chapters unchanged.

## Freshness (accepted limitation)

Metadata only refreshes on explicit refresh (rocket Update) or a cold miss; auto-update stays `chapters_only` by design. Rejected: a TTL-triggered plugin call on page open (would reintroduce a network call in the page path). Upgrade path: drop `chapters_only` from the auto-update pass, or add a config knob.

## Other rules from the audit

- Reset detail/cover must clear the materialized fields or the reset looks like a no-op.
- `POST /api/library` must not write an empty/slug name over a real one.
- Disabled plugin: still serve the stored row; refresh keeps rejecting.
- Migration and entity change MUST be one commit: the new columns against an old DB break every `saved_manga` read.
- "One name" guarantee covers library + detail only; search cards keep the `title` cache.

## Status

Planned only, `openspec/changes/manga-detail-table` (proposal, design D1-D11, deltas on library-save / manga-detail-page / detail-enrichment-fields, tasks 1-5). `openspec validate --strict` clean; no code written.
