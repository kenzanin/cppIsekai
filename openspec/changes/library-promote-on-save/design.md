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

## D5. Unsave wipes the pair; plugins GC by folder, the scheduler GCes by age

**Decision.** `POST /api/library/remove` performs a full pair wipe: one store txn takes the
`saved_manga` row, `chapters`, `chapter_pages`, `reader_positions`, `update_attempts`, `title_meta`
and the pair-scoped host keys (`host/novel|detail_url`, chapters fingerprint); the pair's `api_cache`
rows (exact id + `<slug>:...` page keys via `json_extract`) go on the raw connection; the route then
calls `Cache::purge_pair` for the disk image tree (`<ns>/manga/<slug_dir>` plus covers hashed from
the stored cover URL). No foreign keys: the cascade is one explicit store operation, not schema
machinery.

**Why.** The user-facing contract is "unsave = forget", and re-add must equal a fresh add from
search: empty row, cold envelope cache, cold images. Leaving satellites behind would silently
restore old detail, chosen overlays and the reader position on re-add. Legacy
`host/skip|read|total` keys stay: they are chapter-id-keyed with no slug, and D2 inheritance
attributes them on next sight. The legacy `host/skip|read|total` shadows are swept too, via the
pair's chapter ids (the rows about to die) -- after unsave nothing references the pair, and re-add
starts flag-fresh.

**Consequence.** Unsave is heavier than a one-row delete and destroys curation
(`title_meta.chosen`) by design. Plugin removal keeps `plugin_prune_absent`; `api_cache` aging
keeps `cache_prune_expired` for never-saved browsing dust.

ponytail: purge covers via the saved row's URL only -- a chosen-cover image survives until LRU
eviction; add its URL to the purge list if that ever shows.
## D6. Prune behavior is untouched; only the dependency is removed

**Decision.** `cache_prune_expired` keeps its `fetched_at + ttl + 7d` rule. `pages` stays
`api_cache`-only.

**Why.** Pruning is correct for unsaved browsing and mandatory for signed page URLs (comico's token
decays in minutes; archiving them would serve 410s). After D2+D3 a saved title no longer needs the
rows for detail or chapters, so the prune no longer decides whether a library open hits the network.
