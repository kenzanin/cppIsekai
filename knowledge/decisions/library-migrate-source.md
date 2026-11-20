---
type: Decision
title: "Library migrate-source: repoint, read-boundary carry"
description: "Migrating a saved entry to another source rekeys the row in one txn; read state carries by number boundary, positions/pages do not."
tags: [library, migrate, store, http]
generated: { by: agent/mcp, at: "2026-11-20T17:32:32Z" }
status: stable
---

# Decision

goIsekai's MigrateMangaSource ported as one store txn + one API route (commit d40af5d, 2026-11-21).

## Shape
- `Store::migrate_source(old_plugin, old_slug, new_plugin, new_slug, MigrateTarget)` in `src/store/store_migrate.cpp` — single `TxGuard`; reads BEFORE `begin_transaction` (#5614). Refuses: absent source, target already saved, degenerate pair.
- `POST /api/migrate` (`src/http/http_api_library.cpp`): pre-flight db checks -> cached detail+chapters envelopes (`call_cached`) -> `materialize_chapters` (flags inheritance + fingerprint ride) -> migrate -> `recompute_dup_groups`.
- `GET /view/migrate/<plugin>/<slug>` (`http_pages.cpp`): server shell only; `templates/migrate.mustache` hydrates per-plugin search exactly like search.mustache, EXACT badge from server-injected normalized name+alt-title keys, chapter counts best-effort, `fewer` warn, plain form POST. Detail page Migrate placeholder button -> real link, gated `inLibrary`.

## Carried / not carried
- Carried: added_at (library seniority), pinned, enrichment title_meta (fetched+chosen move to the new pair; a target preview row yields), read chapters by **number boundary**: every target chapter with `number <= max(read source numbers)` becomes read — OR-preserving (previewed target reads stay).
- Not carried (UI warns): per-chapter reader positions, downloaded page files, per-chapter read history granularity beyond the boundary.

## Rationale
Number boundary (not id map): chapter ids are plugin-local and never align across sources; the boundary is the only portable notion of "read up to here". Sub-chapters (6.1) follow their parent because the comparison is numeric.

## Verified
Unit: test_store migrate case, 34 assertions (boundary incl. 2.5 sub-chapter, preview-OR, refusals, meta move). Suite 20/20. Live: scratch save mangahub/one-piece -> /api/migrate -> mangadex row present, old row gone, detail 200, chapters 17; /view/migrate 200, unknown pair 302; scratch rows removed after.
