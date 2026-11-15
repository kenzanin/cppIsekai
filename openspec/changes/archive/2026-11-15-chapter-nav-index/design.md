# Design: chapter-nav-index

## Context

`/api/pages/<plugin>/<slug>/<ch>` (`src/http/http_api_content.cpp:40-128`) does
three things: resolve the chapter id, fetch that chapter's page list through
`call_cached`, and build the response through `build_pages_response`
(`src/http/pages_api.cpp`).

Current resolution, twice:

```cpp
for (const auto& item : *chs)          // list order, first match wins
    if (number == ch) { chapter_id = item.id; break; }
```

and in `build_pages_response`:

```cpp
if (*d == ctx.ch && target_id.empty()) target_id = id;   // first match
else if (*d < ctx.ch && *d > prev)     prev = *d;        // nearest below
else if (*d > ctx.ch && *d < next)     next = *d;        // nearest above
```

So the response can only carry numbers, and numbers are not unique. The reader
already receives the wanted id from the detail page (`?ch_id=`) but only uses it
for `POST /reader/position`; `/api/pages` never sees it.

Live evidence (2026-11-15, six plugins in the library): mangadex 16 duplicate
numbers, mangafreak 5 (list order DESC), mangafire two ids numbered 48. With
number-based navigation each duplicate group is a single stop.

## Goals / Non-Goals

**Goals**

- Every chapter entry in a plugin list is individually reachable from the reader
  buttons, including duplicate numbers.
- Existing number-only URLs keep working unchanged.
- No plugin ABI change.

**Non-Goals**

- Reordering or deduplicating the plugin's list.
- Changing how the detail page lists chapters.
- Distinguishing duplicates visually (title/label comes from the plugin).

## Decisions

**D1. `ch_id` is an optional query parameter on `/api/pages`, not a new route.**
The detail page already produces `/read/...?ch=&ch_id=`; extending the pages
route keeps one URL shape for the reader. A missing or unmatched `ch_id` falls
back to the current first-number-match, so old links, cached responses and
hand-typed URLs behave exactly as before. A new route (`/api/pages/by-id/...`)
would have doubled the resolution logic and the cache keys for no gain.

**D2. The target entry is chosen by id when given, and that id is what the
plugin is asked for.**
When `ch_id` matches a list entry, the `pages` call uses *that* id (not the
id re-derived from the number), the `/cache/img` URLs carry it, and the
response's `chapter` field keeps the number. This is the point of the change:
the reader asked for a specific release.

**D3. Neighbours are resolved by index in a stable number-ordered list.**
`build_pages_response` sorts a copy of the list by `number` with a stable sort
(equal numbers keep plugin order), finds the target's index there, and takes
`prev = order[i-1]`, `next = order[i+1]`. Duplicate group 24a/24b becomes two
consecutive stops. Rejected alternative: sorting by `id` for a tie-break — ids
are opaque (numeric on mangadex/mangafire, `slug:016` on mangafox), so plugin
order is the only meaningful tie-break and stable sort preserves it.

**D4. The response grows `prev_ch_id`/`next_ch_id`; the numeric fields stay.**
`prev_ch`/`next_ch` are what the reader header displays ("Next: 24") and what
read-ahead uses for logging; dropping them would churn the template and the
tests. Ids are additive, so a client that ignores them sees no difference.

**D5. The reader navigates by id end to end.**
`loadChapter(n, page, {chId})` passes `ch_id` to `/api/pages`, `chUrl` writes
`ch_id` into the address bar (so refresh and back/forward keep the exact
release), and `_prefetch` is keyed by id instead of number — two entries
numbered 24 would otherwise overwrite each other in that map. `ch_id` from the
detail link is the initial value; `prev_ch_id`/`next_ch_id` update it on every
switch. The position store already keys chapter id, so resume is unaffected.

**D6. Cache keys follow the new parameter for free.**
`call_cached` hashes `plugin|method|canonical-params`; adding `ch_id` to the
`pages` params object gives each duplicate its own `api_cache` row. The
`chapters` call stays keyed by title id only, so the list is still fetched once
per title. No response-shape version bump is needed (the payload only gains
fields).

**D7. Tests pin the duplicate case, not just the ordering.**
`tests/test_pages.cpp` gets a fixture with two entries numbered 24 and one
numbered 23/25: assert `prev`/`next` by index for both 24s, assert
`prev_ch_id`/`next_ch_id` carry the sibling's id, and assert the number-only
path still resolves to the first entry. The existing descending-list test stays.

## Risks / Trade-offs

- [Reader state has two identifiers now (`ch` number + `chId`)] → the number is
  display/URL sugar; every server call takes the id. Keep `ch` derived from the
  response so a stale id can never desync the label.
- [A plugin that reorders its list changes which duplicate is "first"] →
  navigation is consistent within one served list; across list refreshes a
  reader may see the group in the other order. Acceptable: ids still resolve.
- [Old bookmarked `?ch=` URLs] → D1 fallback keeps them working.
- [`prev_ch_id` when the neighbour is the same number as current] → the reader
  must compare ids, not numbers, when deciding whether a neighbour exists;
  the template guards on the id string.

## Migration Plan

1. Route: parse `ch_id`, prefer it (D1, D2).
2. `build_pages_response`: stable order, index neighbours, id fields (D3, D4).
3. Reader template: id-based switch, URL, prefetch (D5).
4. Tests (D7), build, full suite.
5. Live: mangadex title with duplicates — read both entries of a duplicated
   number, confirm `next`/`prev` reach the sibling; mangafreak 24a/24b; then a
   number-only URL to confirm the fallback.
