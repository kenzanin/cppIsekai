# Proposal: chapter-nav-index

## Why

`/api/pages/<plugin>/<slug>/<ch>` resolves the target chapter by **number**, and
picks the **first** list entry carrying that number. Plugin chapter lists carry
duplicate numbers — mangadex lists 16 duplicated numbers today, mangafreak has
5 (24a/24b, 9a/9b, 8a/8b, 7a/7b, 6a/6b), mangafire lists chapter 48 twice
(two upstream ids, five minutes apart). Consequences:

- The reader can read a duplicate release only if the detail row happened to link
  its id; `prev_ch`/`next_ch` are numbers, so navigation always lands on the
  first entry with that number. The sibling version is unreachable from the
  reader buttons.
- `prev_ch`/`next_ch` are computed as "nearest number below / above", which
  collapses a whole duplicate group into one navigation point: 24a and 24b are
  one stop, not two.
- The detail page already links `/read/...?ch=<number>&ch_id=<id>`
  (`templates/detail.mustache:182`), but `/api/pages` ignores `ch_id`: the
  reader re-resolves the chapter id client-side for the position store only.

The navigation fix landed in f8bcbc9 (nearest numeric value instead of list
order) made navigation *correct* for unordered lists, but duplicate numbers are
still one node. Reaching each release needs index-based navigation, which needs
the chapter id in the pages response.

## What Changes

- `GET /api/pages/<plugin>/<slug>/<ch>` accepts an optional `ch_id` query
  parameter. When it matches a list entry, that exact entry is the target
  (plugin `pages` call, cache-key, `/cache/img` `ch_id`); when it is absent or
  unknown, behaviour is unchanged (first entry whose number matches).
- `build_pages_response` orders the chapter list once (stable sort by
  `number`) and resolves `prev`/`next` by **position in that order**, so a
  duplicate group becomes several consecutive stops instead of one.
- The response gains `prev_ch_id` / `next_ch_id` (strings) next to the existing
  `prev_ch` / `next_ch` numbers. The numbers stay for display; the ids are what
  the reader navigates with.
- `templates/reader_script.mustache` navigates by id: `loadChapter` passes
  `ch_id`, `chUrl` keeps `ch_id` in the address bar, `_prefetch` is keyed by id.
- No change to `/api/chapters` order, the detail page chapter list, or the
  position store (already id-keyed).

## What does not change

- `/api/pages` response shape for existing fields: `plugin`, `slug`, `chapter`,
  `pages[]`, `prev_ch`, `next_ch` stay as they are.
- Plugin ABI: no new method, no new host tool, no manifest field.
- Reader modes, zoom, read-ahead window, position semantics.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `reader-page`: pages API takes an optional chapter id and returns id-based
  neighbours; the reader switches chapters by id, not by number.

## Dependencies

- Assumes the f8bcbc9 nearest-numeric fix (already on main) as the fallback
  path for number-only links.

## Impact

- `src/http/http_api_content.cpp` — parse `ch_id`, prefer it when resolving the
  chapter id.
- `src/http/pages_api.{hpp,cpp}` — `PagesCtx.chapter_id`, index-based
  neighbours, `prev_ch_id`/`next_ch_id` in the response.
- `templates/reader_script.mustache` — id-based switch, URL sync, prefetch key.
- `tests/test_pages.cpp` — duplicate-number fixtures.

## Out of scope

- Deduplicating plugin lists (the site really does list both releases).
- Labeling duplicate entries differently in the reader chrome (chapter title is
  plugin data and often empty; mangafreak titles are `Chapter 24` for both).
- `reader::next_window` (already sorts ascending and is chapter-scoped).

## Success criteria

- With a list containing two entries numbered 24, reading the first one's
  `next` lands on the second, and reading the second one's `prev` lands on the
  first.
- `/api/pages/...?ch_id=<id>` serves that exact entry's pages.
- Number-only links (no `ch_id`) behave exactly as before the change.
- Full suite green; live check on mangadex (16 duplicates) and mangafreak (5).
