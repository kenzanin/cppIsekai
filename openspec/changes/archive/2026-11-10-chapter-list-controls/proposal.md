# Proposal

## Why
The detail-page chapter list is a single unpaged wall (100+ rows on long runners) and its rows are dead weight: no per-chapter skip control, no read-completion status, no progress counter. The reader already tracks position and skip-aware windows; the list never surfaces any of it.

## Changes
- `src/http/http_api.cpp`
  - `POST /chapter/<id>/read` `{page}` — record pages read under `host/read/<id>` in plugin_state (mirrors `host/skip/`); `0` clears.
  - Auto-hook in `POST /reader/position`: bump the chapter's record to `max(stored, page)` — the check lights up from real reading, no reader UI change.
  - `POST /chapter/state` `{ids:[...]}` -> `{<id>: {skip, read}}` map; one fetch paints eye, check and counter for the whole list. Skip writes reuse the existing `POST /chapter/<id>/skip`.
- `templates/detail.mustache`
  - Row: eye-svg skip toggle, chapter-number + title linking into the reader, date, `read/total` counter, double-check svg read toggle, cbz — in that order.
  - Client-side pagination over the already-loaded chapter array (prev/next + n/m, hidden on a single page).

## Capabilities
### New Capabilities
- `chapter-list-controls`: per-chapter skip/read controls, read progress, and pagination on the detail chapter list

### Modified Capabilities
<!-- none: reader-fullscreen / detail-enrichment keep their requirements; this adds list-level controls -->

## Impact
`src/http/http_api.cpp` (~+50), `templates/detail.mustache` (row + pager). Store untouched (plugin_state KV, reserved `host/` namespace). Rollback = revert commit.
