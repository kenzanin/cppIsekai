---
type: decision
title: "Saved manga detail lives on saved_manga (planned, openspec manga-detail-table)"
description: "Planned source of truth for a saved manga's display metadata: the saved_manga row itself (name, cover, synopsis, status + new authors, artists, tags), not a new manga_detail table. The detail resolver (apply_chosen + ensure_name) writes resolved values through to the row; /api/detail then answers from the row and only calls the plugin on explicit refresh or cold miss. Enrichment candidates/chosen stay title_meta-only. Not implemented: proposed as openspec/changes/manga-detail-table."
tags: [detail, saved_manga, store, openspec, source-of-truth, planned]
generated: { by: agent/mcp, at: "2026-11-15T02:52:15Z" }
status: stable
---

## Decision

A saved manga's display detail lives on `saved_manga` itself: `name`, `cover`, `synopsis`, `status` plus new `authors`, `artists`, `tags`. No new `manga_detail` table.

## Write path

The detail resolver (`apply_chosen` + `ensure_name`, src/http/enrich_internal.hpp) writes the resolved, user-visible values through to the row, skipping the write when unchanged. Enrichment fetches and `chosen` saves still touch `title_meta` only.

## Read path

`/api/detail` answers a saved manga from its row without calling the plugin; `POST /api/refresh` re-asks the plugin and rewrites the row. Chapter data keeps its live/stale-serve path. Unsaved titles stay live (no row to key on).

## Status

Planned only. Captured as `openspec/changes/manga-detail-table`: proposal.md, design.md (D1 row over new table, D2 write-through in the resolver, D3 read-first, D4 title_meta keeps provenance, D5 additive migration with lazy population), spec deltas on `library-save`, `manga-detail-page`, `detail-enrichment-fields`, and tasks.md. `openspec validate manga-detail-table --strict` is clean; no code written yet.

# Related Concepts
- [Detail vs library title mismatch: fixed (glaze map insert invalidated the title view)](../findings/detail-name-vs-library-card.md): The 2026-11-15 empty-detail-name bug is what motivated storing resolved detail on saved_manga: the resolver ran but nothing durable held its output, so the next page open re-derived the name.
