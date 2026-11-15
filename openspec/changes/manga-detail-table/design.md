# Design: manga-detail-table

## D1. The detail row is `saved_manga`, not a new table

**Decision.** Display detail columns are added to the existing `saved_manga` row instead of a new
`manga_detail` table.

**Why.** The set of titles whose metadata is worth persisting is exactly the set the user saved;
an unsaved manga has no row to key on and its detail stays live. A second table would need the same
`(plugin, slug)` key, a join on every card render, and its own lifecycle (delete on unsave). The
library grid already selects this row, so the columns arrive where they are read.

**Consequence.** `saved_manga` widens by `authors`, `artists`, `tags`. Detail for a non-saved title
is necessarily ephemeral — acceptable, since nothing else renders it.

## D2. Write-through happens in the resolver, once per resolve

**Decision.** The detail API persists the merged display fields right after `apply_chosen`
returns a usable envelope, and only for a saved row.

**Why.** `apply_chosen` is already the single place that knows the precedence (chosen title →
scraper title → saved name, plus tag add/hide and cover override). Storing its output there means
there is no second implementation of the merge that could disagree with the API response. Writing
at fetch time instead would store un-chosen scraper data, which is not what the user sees.

**Consequence.** One extra sqlite UPDATE per detail open of a saved manga that changed. The write
is skipped when the resolved fields equal the stored ones, so a plain page open is read-only.

## D3. Read-first, plugin only on refresh

**Decision.** When a saved row has a complete display detail, `/api/detail` answers from the row
(plus `title_meta` enrichment metadata) and does not call the plugin. `POST /api/refresh` calls the
plugin, rewrites the row and the `api_cache` envelope.

**Why.** Local-first is this project's default: `api_cache` already answers without the network and
only a cold miss reaches a plugin. Reaching a plugin on every detail open made the display depend
on an upstream that is frequently empty or blocked; the library row is the durable copy.

**Consequence.** A saved manga whose upstream changes its title only updates when the user presses
⟳ Update (or auto-update runs). Chapter lists, which are the freshness-sensitive part, stay
stale-served as today.

## D4. `title_meta` keeps provenance; the resolved values live on the row

**Decision.** `title_meta.fetched` (raw candidates per source) and `chosen` (the user's picks) stay
exactly as they are. The `detail-enrichment-fields` rule "enrichment code SHALL NEVER write
`saved_manga`" is narrowed to candidates: fetching a source or saving `chosen` still writes only
`title_meta`, while the resolver materializing the merge may write the row.

**Why.** The rule existed to stop enrichment from clobbering the source cache and the user's saved
entry with unselected candidate data. Per-field user control is preserved — only the resolved,
user-approved values are materialized, and reset paths (`Reset detail`, `Reset cover`) resolve
again from the remaining state.

**Consequence.** Two writes per user-picked change (row materialization + `title_meta`), both
idempotent. Resetting detail must clear the materialized fields so the next resolve repopulates
them from the scraper envelope.

## D5. Additive migration, lazy population

**Decision.** Columns are added with `DEFAULT ''`; existing rows are not backfilled in the
migration.

**Why.** Backfilling requires a plugin call per saved row, which the change cannot do inside a
schema migration, and a NULL vs empty distinction would leak into every reader.
`additive ALTER TABLE` is the pattern this store already uses for `synopsis`, `alt_titles`,
`status_flags`.

**Consequence.** A pre-existing saved row reads as "detail not stored yet" and is populated on its
next detail open. Nothing breaks in between: the resolver already falls back to the live envelope
and the saved name.
