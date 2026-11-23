# Design: library-promote-on-save

## D1. `api_cache` is the staging store — no new temp table, no JSON1 column

**Decision.** Promotion reads the existing `api_cache` rows. No staging table, no JSON1/JSONB column
on `saved_manga`, no envelope copy anywhere else.

**Why.** `api_cache` already holds the raw envelope as JSONB, already keyed per `(plugin, method,
params)` with a canonical hash, already written by the open that precedes every save (the library
button lives only on the detail page). A second staging store would add a migration, a lifecycle, and
a second copy to keep in sync with the same prune scheduler — all to hold bytes that are already on
disk.

**Consequence.** Promotion shares `api_cache`'s lifetime. That is exactly why D3 exists: the read path
must not depend on the staging store for saved titles.

## D2. Promotion runs in the save route's db task, reusing the existing materializers

**Decision.** `POST /api/library` resolves the detail/chapters cache keys for the slug, reads both
rows, and calls the existing `materialize_detail`, `materialize_chapters` and `sync_saved_stats` for
that pair, then `save_manga`. Order matters: materialization needs the row to exist
(`saved_detail_put` is a no-op for absent rows), so the upsert runs first, then the promotion, inside
one `on_pool(db, ...)` call.

**Why.** The helpers are the single write path for these columns and already carry the name guard
(D9), the unchanged-write skip, the C3 chapter fingerprint and the flag composition. Writing a
promotion-specific writer would fork all four rules.

**Consequence.** Promotion is idempotent and silent on failure: missing/corrupt cache rows leave the
saved row exactly as today, and the existing lazy path resolves on the next open. A plugin is never
called during a save (an add with no cache stays lazy rather than paying a network round trip).

## D3. Chapters are served from `chapters` rows (chapter-store becomes a read source)

**Decision.** `GET /api/chapters/<plugin>/<slug>` answers from the stored rows when any exist for the
pair: envelope `{ok:true, cached:true, data:[{id, number, title, url, release_at}]}`, ordered by
number. `call_cached` remains the path for pairs with no rows.

**Why.** Without it, promotion of the chapter list is pointless: `call_cached` would still be the
reader, and the list would revert to the network after `cache_prune_expired`. The rows already exist
(written on every parse, including pre-save opens), already survive restart, and are the store the
chapter flags key on — so making them the read source removes a duplicate cache rather than adding
one.

**Consequence.** A row-served list can lag the site until ⟳ Update (which overwrites `api_cache` and
re-materializes rows through `refresh_title`). Accepted: this is the same freshness contract
`manga-detail-table` D3 already set for metadata.

## D4. `chapters.release_at`, verbatim text, inside the seed fingerprint

**Decision.** Add `release_at TEXT NOT NULL DEFAULT ''` to `chapters` (additive `ALTER TABLE` next to
the existing zoom/status_flags migrations) and store whatever the plugin emitted — epoch seconds,
ISO date string, or empty. Include it in `chapters_fingerprint`.

**Why.** The detail page renders `ch.release_at`; a row-built envelope without it silently blanks the
date column. Storing it verbatim (rather than normalizing to epoch) keeps the existing
`fmtDate` dual-shape handling valid and avoids a parser at the trust boundary. The fingerprint must
cover it or a release-date-only refresh would be short-circuited as "unchanged seed set".

**Consequence.** One column, one migration, no backfill: older rows read `''` until the next parse.

## D5. `saved_manga` is master of user state; satellites stay un-referenced caches

**Decision.** No foreign keys from satellite tables to `saved_manga`, no cascade on unsave. Unsave
deletes the saved row (and its FTS entry) only.

**Why.** These tables are keyed by the work's identity `(plugin, slug)`, not by a library
membership; a chapter's read flag, a reader position and an FTS signal are meaningful before and
after a save. A cascade would throw away exactly the data that makes re-adding instant and would
need a composite-string-FK migration on a table sqlite_orm cannot `ALTER` cheaply. Plugin removal
(`plugin_prune_absent`) already GCs every per-plugin table, which is the only deletion that must be
complete.

**Consequence.** A title that is opened and never saved leaves `chapters`/`api_cache` rows behind —
bounded by the existing prune for `api_cache` and by the library's own small scale for the rest. If
that ever hurts, the upgrade path is one scheduler-tick `DELETE ... WHERE NOT EXISTS (saved_manga)`,
not a schema change.

## D6. Prune behavior is untouched; only the dependency is removed

**Decision.** `cache_prune_expired` keeps its `fetched_at + ttl + 7d` rule. `pages` stays
`api_cache`-only.

**Why.** Pruning is correct for unsaved browsing and mandatory for signed page URLs (comico's token
decays in minutes; archiving them would serve 410s). After D2+D3 a saved title no longer needs the
rows for detail or chapters, so the prune no longer decides whether a library open hits the network.
