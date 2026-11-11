# Proposal

## Why
The detail page's **Fetch detail** wizard can enrich a saved manga with metadata from the enrichment plugins (MangaDex, MangaUpdates), but there is no way to keep the *related reading* recommendations those same plugins return. Today they render transiently in a "Related" section that disappears on reload unless the scraper plugin itself implements `related` (atsumaru does not; mangadex does). Users who want a reading trail on the detail page must add every recommendation to the Library one by one — tedious when both enrichment sources surface up to 10 candidates each.

## What Changes
- **Enrichment recommendations**: bump both enrichment plugins' recommendation cap from 5 to **10** items (`while #scored > 10` / `while #recs > 10`). Covers and other capped lists stay at 5.
- **Detail wizard — "related readings" segment**: the Fetch-detail panel gains a read-only list of merged recommendations from the enrichment sources shown so far (MangaDex first, MangaUpdates only when no normalized duplicate), plus **Add all** / **Remove all** buttons — no per-item toggles, mirroring how tags work but batched.
- **Persistence**: `chosen.related_add` (new `normalize_chosen` whitelist key; array of `{name, url, id?}`) stores the wizard's selected related readings for this manga. `title_meta.chosen` remains the only write target; `saved_manga` and `api_cache` are untouched.
  - **Add all** = overwrite `related_add` with the merged wizard list (replace, not append).
  - **Remove all** = drop `related_add` entirely.
  - The scraper plugin's own `related` output is never touched by these buttons.
- **Detail page Related section** = scraper plugin `related` items (existing behavior) **appended** with `chosen.related_add`, deduped by normalized `url||id||name` (first wins → scraper wins, then MangaDex, then MangaUpdates). Hidden entirely when both are empty.
- Non-goals: no Library entries are created for recommendations; no chapter/reading material is fetched from MD/MU; the section stays text-only links to the source site (external tab).

## Impact
- Affected specs: `detail-enrichment-fields` (new requirement for related-reading selection in the wizard + merged Related rendering).
- Affected code: `plugins_dev/lua/enrichment-{mangadex,mangaupdates}/main.lua`, `src/http/enrich_internal.hpp` (whitelist + merge), `src/http/http_api_enrich.cpp` (unchanged routes; `related_add` rides `normalize_chosen`), `templates/detail.mustache` + `templates/detail_script.mustache`, golden fixtures.
- No new dependencies; no DB schema change (JSON in `chosen`).
