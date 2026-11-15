# Design: manga-detail-table

> Revised 2026-11-15 after a code audit against the running daemon. R1–R11 in the review
> correspond to the decisions below: D3 (freshness), D5 (migration), D6 (reset), D7 (picker
> parity), D8 (`cached`), D9 (upsert guard), D10 (disabled plugin), D4 (source vs merged
> synopsis), D11 (name scope).

## D1. The detail row is `saved_manga`, not a new table

**Decision.** Display detail columns are added to the existing `saved_manga` row instead of a new
`manga_detail` table.

**Why.** The set of titles whose metadata is worth persisting is exactly the set the user saved; an
unsaved manga has no row to key on and its detail stays live. A second table would need the same
`(plugin, slug)` key, a join on every card render, and its own lifecycle (delete on unsave). The
library grid already selects this row, so the columns arrive where they are read.

**Consequence.** `saved_manga` widens. Detail for a non-saved title stays ephemeral — acceptable,
since nothing else renders it.

## D2. Write-through lives in a route-level helper, never in `apply_chosen`

**Decision.** A helper `materialize_detail(store, plugin, slug, resolved_env)` persists the resolved
display fields. It is called from `/api/detail` (after `apply_chosen`), from `refresh_title` when it
ran the detail method, and nowhere else. `apply_chosen` stays a pure merge.

**Why.** `apply_chosen` is also called by `POST /api/enrichment/chosen` paths and by
`tests/test_http_enrich.cpp`, whose guard case asserts that enrichment traffic never touches
`saved_manga`. Putting the write inside the merge would break that contract and that test for every
caller. The route is the only place that knows the response actually served a detail envelope.

**Consequence.** One caller to keep in sync, one place to mock in tests. The write is skipped when
the resolved fields equal the stored ones, so a plain page open stays read-only.

## D3. Read-first with explicit freshness rules

**Decision.** When a saved row holds display detail, `/api/detail` serves the row (plus the
`enrichment` block from `title_meta`) and does **not** call the plugin. Metadata grows stale until
the user presses ⟳ Update (full refresh: detail + chapters) — auto-update keeps running
`chapters_only` and does not fetch detail.

**Why.** Local-first is this project's rule, and the prior behaviour had no reliable metadata
refresh either: the detail route only reached the network on a `api_cache` cold miss, so metadata
age depended on cache expiry. Making the row the answer removes the plugin from the page path
entirely; the ⟳ button already exists as the explicit freshness control. Adding a TTL-triggered
plugin call on page open was considered and rejected: it would reintroduce a network call on a
page view, which `stale.hpp` deliberately avoids.

**Consequence (accepted limitation).** A saved manga whose upstream changes its name/cover keeps the
stored values until ⟳ or a re-resolve. This is now a documented rule, not an accident. Upgrade path
if it becomes annoying: drop `chapters_only` from the auto-update pass (cost: one detail call per
interval per saved title) or add a config knob for open-triggered refresh.

## D4. Source synopsis stays source; the merged synopsis gets its own column

**Decision.** `saved_manga.synopsis` and `alt_titles` keep their current meaning ("plugin source
envelope, never enrichment") and keep being written by `saved_signal_update`. The resolved display
synopsis is stored in a new column `synopsis_display`.

**Why.** `sync_saved_stats` runs on every detail open and overwrites `synopsis` from the envelope
(`src/http/api_ctx.hpp`). Reusing that column for the merged value would silently revert the user's
chosen synopsis on the next open, and would also perturb `recompute_dup_groups`, which clusters on
`synopsis`/`alt_titles`.

**Consequence.** Two synopsis columns with different meanings; the detail page and API read
`synopsis_display` (falling back to `synopsis` when empty). Clustering output is unchanged.

## D5. Additive migration in the same commit as the entity change

**Decision.** `authors`, `artists`, `tags`, `synopsis_display` are added with `DEFAULT ''` in the
existing additive migration block, committed together with the `SavedMangaEntity` / `make_table`
change.

**Why.** sqlite_orm generates `SELECT`s from the entity's column list. A build carrying the new
entity against a database without the columns fails **every** `saved_manga` read at once — library
grid, stats rollups, duplicate groups and the auto-update pick. This is a hard switch, not a
degradation, so the two changes must not be separable.

**Consequence.** No backfill: a pre-existing row reads as "detail not stored yet" and is populated
on its next open. A row whose plugin is gone keeps serving whatever it has.

## D6. Reset clears the materialized fields

**Decision.** `POST /api/enrichment/reset` (`mode=detail|cover|all`) also clears the materialized
display fields it invalidates, so the next open re-resolves them from the scraper envelope.

**Why.** Today the route only touches `title_meta`. With read-first, clearing `chosen` while the row
keeps the merged values makes `Reset detail` and `Reset cover` look like no-ops — a real regression
of an existing feature.

**Consequence.** Reset keeps `title_meta.fetched` (per its own spec) and additionally stamps the row
so the next open resolves.

## D7. Read-first responses keep the enrichment block

**Decision.** A row-served `/api/detail` response still carries `enrichment: {fetched_at, chosen}`
and `chosen_cover`, exactly like today's merged envelope.

**Why.** The detail page's picker hydrates `d.enrichment`, `enrichLastFetched`, `hiddenTags`,
`chosenNow()` and the `Reset cover` button from those keys. Building the response from the row alone
would make the picker believe nothing is chosen while `title_meta` still holds it.

**Consequence.** The read-first response is "row values + title_meta overlay", not "row only". `release_at` has no stored column, so a row-served page hides the release badge; the next live pass (⟳ or cold re-resolve) restores it.

## D8. A row-served answer is flagged `cached`

**Decision.** `/api/detail` responses served from the row set `cached: true`.

**Why.** The page shows the `cached` chip and uses the flag to make ⟳ meaningful. The row is the
offline copy by definition; claiming freshness it did not verify would misreport where the data came
from.

**Consequence.** Saved manga detail pages show `cached` most of the time. That is honest and
matches the ⟳-is-the-freshness-control model of D3.

## D9. The library upsert never writes a placeholder name

**Decision.** `POST /api/library` keeps writing `name`/`cover`, but skips the name write when the
submitted name is empty or equals the slug, and never overwrites a non-empty stored name with one of
those.

**Why.** The detail page submits `detail.name || slug`. A pre-migration row with no stored detail,
opened while the plugin is unreachable, would submit the slug and overwrite the row's name — the
same bug class the 2026-11-15 resolver fix closed.

**Consequence.** One guard in the route; re-adds still refresh a real name/cover.

## D10. Disabled plugins serve the row, refresh stays refused

**Decision.** For a saved manga whose plugin is disabled, `/api/detail` still serves the stored
detail; `POST /api/refresh` keeps returning `{"ok":false,"error":"plugin disabled"}`.

**Why.** A disabled plugin means "stop reaching the network", not "hide what was already saved".
`call_cached` already answers disabled plugins from cache for unsaved titles; the saved row is the
same idea with a better answer. Refresh is the only path that would reach the network, and it must
stay closed.

**Consequence.** An explicit scenario in `manga-detail-page`; auto-update already skips disabled
plugins.

## D11. "One name" means library and detail, not search results

**Decision.** The shared-name guarantee covers the library grid cards, `/api/library` and the detail
heading. Search-result cards keep reading the `title` search-cache table.

**Why.** The `title` table is a separate, TTL-bound search cache with its own names; folding it into
`saved_manga` would couple search to the library and change search semantics.

**Consequence.** The proposal no longer claims a global single name; the mismatch class that
motivated this change (library vs detail) is what closes.
