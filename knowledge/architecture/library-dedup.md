---
type: Fact
title: "library-dedup: signal columns on saved_manga; site plugins carry description, not synopsis"
description: "library-dedup ships columns synopsis/alt_titles/dup_group on saved_manga; source-envelope sync reads synopsis with 'description' fallback because mangadex/atsumaru detail envelopes expose only 'description'; alt_titles appears only in enrichment plugins, stored verbatim when present in source envelope."
tags: [library, dedup, store, fts5, plugin-envelope]
generated: { by: agent/mcp, at: "2026-11-11T22:18:31Z" }
status: draft
---

In-progress 2026-11-12 (openspec change `library-dedup`). Durable facts from tasks 1.1/1.2:

- `saved_manga` += `synopsis TEXT ''`, `alt_titles TEXT ''` (JSON array, verbatim), `dup_group INTEGER 0`. Boot `pragma.table_info` + `ALTER TABLE ADD COLUMN` probe in `store_core.cpp` (same ponytail pattern as the stats columns).
- Site plugins (mangadex, atsumaru) detail envelopes carry ONLY `description`, never `synopsis` or `alt_titles`; those exist solely in the enrichment plugins. `sync_saved_stats` reads `synopsis` first and falls back to `description`; `alt_titles` is re-serialized verbatim via `envelope_raw_json` when present in the SOURCE envelope (current site plugins emit none, column stays ''). Enrichment values deliberately NOT used for dedup (per user directive).
- `saved_signal_update(plugin, slug, synopsis, alt_titles_json)` store helper: empty args leave the stored field untouched; bumps `updated_at` only on a real change.
- `save_manga` preserves synopsis/alt_titles/dup_group on re-save (signal columns are owned by the envelope sync, never by the plain save upsert).

FTS5 + Dice clustering not implemented yet at time of writing. See `openspec/changes/library-dedup/design.md` D1-D6.
