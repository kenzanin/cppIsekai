# Design: library-ownership

## D1. Ephemeral browse data is an in-process map, not a SQLite temp table

**Decision.** `StoreImpl` gains an `Ephemeral` struct: pairs -> (chapter rows with flags/read_max,
page URLs, reader position, title_meta, per-pair host fields), plus an LRU bound on pair count.
No DDL, no second connection, no temp database.

**Why.** Every store method already opens a `store_detail::OpTrace`, which holds the process-wide
recursive `db_mutex` for the whole call, so a plain `std::map` is serialized by machinery that exists.
(Note: store calls are *not* thread-confined — `call_cached` reaches `plugin_enabled`/`api_cache_*`
from a plugin-pool thread — so the mutex, not the thread, is what makes the map safe.) A
`CREATE TEMP TABLE` would add DDL, prepared statements and bind marshalling for five shapes that
already have typed structs, and would still share the connection's transaction/busy semantics.
"Gone on restart" is satisfied by both; the map is strictly less code.

**Consequence.** Browse state dies with the process (D4). Memory is bounded by the LRU pair cap.

## D2. Routing lives inside Store; no caller changes

**Decision.** Each pair-keyed store method asks `saved_exists(plugin, slug)` and dispatches to the
SQLite implementation or the map. Signatures, return types and exceptions stay identical.

**Why.** ~16 call sites (routes, reader window, prefetch worker, CBZ export, scheduler) treat the store
as one thing. Branching there would spread the same `if` over five modules. One predicate, one place.

**Consequence.** `saved_exists` runs on the already-hot store path; it is a PK lookup and the pair set
is small. Behaviour differences (persistence) are documented per method.

## D3. Save promotes browse state, including flags and read_max

**Decision.** Promotion moves, in one transaction: chapter identities + `flags` + `read_max`, page URL
rows, reader position, title_meta (fetched/chosen), and the per-pair host fields; then erases the pair
from the map. The detail envelope path (`apply_chosen` -> `materialize_detail`) keeps running from
`api_cache` as today.

**Why.** The browse rows are the same rows, just cheaper storage. Dropping them on save would silently
lose the exact state the user just built (read 5 chapters, then add the title).

**Consequence.** Save does more writes than a row insert; it stays one transaction and one db task.
Promotion reads the map **directly**, never through the routed readers: the moment the `saved_manga`
row exists the routing flips to SQLite, so a routed read during promotion would silently see an empty
pair.

## D4. Restart drops browse state

**Decision.** Accepted: after a daemon restart, unsaved titles start from zero (no flags, no position).
Saved titles are unaffected.

**Why.** This is the requested contract — the ephemeral store is the price of never having orphans.

**Upgrade path (ponytail).** If losing a long unsaved read annoys: persist only `reader_positions` for
unsaved pairs (one row per pair, cannot become an orphan by construction) and hydrate the map from it.

## D5. Orphan is derived from the registry, not stored on the row

**Decision.** A saved pair's upstream state is computed at read time:
`live` = registered and enabled, `offline` = registered and disabled, `orphan` = not registered.

**Why.** A stored flag needs a writer on install, a writer on removal and a repair path when either is
missed; a registry rebuild (folder rename, scan crash, mount glitch) would leave stale bits. Deriving
has one source of truth and makes reinstall an instant revive with zero writes.

**Escape hatch.** If a cheap sorted/filtered query over orphans is ever needed, bit7 of
`status_flags` is free and can mirror the derived value on scan — but the registry stays authoritative.

## D6. Plugin removal GCs caches and registry, never library data

**Decision.** `plugin_prune_absent` keeps its transaction but re-scopes it: delete `title`, `api_cache`
and `plugin_state` rows for that plugin id and the `plugin` row; never touch `saved_manga`,
`chapters`, `chapter_pages`, `reader_positions`, `title_meta` or `update_attempts`.

**Why.** The registry is hosting state; the library is user data. Deleting a folder must be reversible,
and implementation swaps (Lua <-> JS <-> WASM) assume the data outlives the runtime.

**Consequence.** Orphan rows stay until the user purges or migrates them (D8). `update_attempts` stays
so backoff history survives a reinstall.

## D7. "Has upstream" replaces "enabled" in the gates

**Decision.** `plugin_enabled()` currently returns **true** for an unregistered id ("nothing to gate").
Add `plugin_upstream(plugin_id) -> {live, offline, orphan}` and gate on `live` in: the stale layer
(no plugin call), `refresh_title` (answer `plugin removed`), and the scheduler candidate query
(`COALESCE((SELECT enabled ...), 1) = 1` becomes an EXISTS on a live plugin).

**Why.** Otherwise an orphan keeps being picked for auto-update and stamps errors/backoff forever, and
a cold miss would attempt a call that cannot succeed.

**Consequence.** Orphan entries behave exactly like offline ones from the UI's point of view.

## D8. Orphans are visible, filterable, migratable, purgeable

**Decision.** `GET /api/library` adds `state` (`live|offline|orphan`) per row; the grid renders an
"orphan" badge and filter; `POST /api/library/purge-orphans` deletes every orphan pair through the
existing wipe path (rows + `api_cache` + disk images). Migration needs no change.

**Why.** Retention without an exit would be a data leak. Filtering by state is also the discovery hook
for "move these to the reinstalled plugin".

**Consequence.** One more route and one more grid filter; the wipe path is reused, not duplicated.

## D9. `api_cache` stays in SQLite and stays outside the pair tree

**Decision.** No change to `api_cache` or its prune.

**Why.** Its key is a request shape (plugin + method + params) and search rows have no manga identity at
all, so it cannot be a child of `saved_manga`. Keeping it durable also keeps stale-serve and offline
reads working across restarts.

## D10. Per-pair host fields become master columns

**Decision.** `host/chapters_fp/<p>/<s>`, `host/novel/<p>/<s>` and `host/detail_url/<p>/<s>` become
`saved_manga.chapters_fp`, `.novel`, `.detail_url`; `host/pos` (no readers) and the `title` table plus
`store.search/count` are deleted.

**Why.** 350 of 380 `plugin_state` rows were per-pair copies: a key-value table pretending to be
columns. After the move `plugin_state` holds only plugin-owned global state (`host/pos` was dead, the
`title` table had zero rows).

**Consequence.** Boot migration copies the values into the new columns once, then deletes the keys and,
if it is empty, the table's remaining dead rows. The search cache paths die with `title`
(`search-page` capability already renders from the plugin envelopes).

## D11. Implementation swap keeps data; image cache is display-name keyed

**Decision.** Do not change cache namespacing in this change, but document the effect: the image cache
directory derives from the plugin's **display name** (`cache_ns`), so a swap that renames the plugin
loses its cached image bytes (refetchable online; data and library untouched).

**Why.** Data identity is the plugin id and must stay stable; image bytes are a cache and may be
re-fetched. Coupling the two would force a data migration for every rename.

**Upgrade path.** Key the image namespace on plugin id when image retention across renames matters.
