# Proposal: cbz-chapter-order

## Why

`cbz::build` accumulates entries in a `std::map<std::string, fs::path>`, so archive order is
lexicographic on the entry name `chNNN/pNNN.ext`. `cache_internal.hpp` pads a chapter number
to a **minimum** of three digits, so a four-digit chapter produces `ch1000`, which is a
different width and sorts **before** `ch999`.

Proven, not inferred. A probe test cached `ch999/p001.webp` and `ch1000/p001.webp`, exported
the slug, and read the local file header offsets out of the returned zip:

```
ch999  at offset 154
ch1000 at offset  67
```

Chapter 1000 therefore lands ahead of chapter 999 — the archive opens in the wrong reading
order for any title past 999 chapters. Every source with a four-digit chapter count is
affected, and the failure is silent: the zip is valid, only the order is wrong.

The main spec carries the same defective assumption. `cache-layout` "CBZ export route"
states that zero-padded names make "lexicographic order inside the archive equal reading
order". Zero padding only preserves order up to the padded width, so the requirement
itself licenses the bug.

## What Changes

- Order archive entries by **numeric** chapter then numeric page instead of by the string
  image of those numbers. The entry names stay `chNNN/pNNN.ext`; names are part of the
  cache layout and are not the defect — only the sort key is.
- Drop the claim that zero padding makes lexicographic order correct, and state the numeric
  ordering requirement instead.
- Correct the requirement's scope: the route exports cached **chapter page images**. It has
  never included the cover (v3 cover files are content-hash named and carry no slug
  mapping at the filesystem level), so the current wording promises an entry the code does
  not produce.
- Add a regression test that fails on the current implementation, locking order for a
  chapter count that crosses the padding width.

## Capabilities

### New Capabilities
_(none)_

### Modified Capabilities
- `cache-layout`: the CBZ export route requirement replaces "lexicographic order equals
  reading order" with numeric ordering, and drops the cover entry from its scope.

## Impact

- `src/http/export_cbz.cpp` — entry container and sort key.
- `tests/test_export.cpp` — new regression case.
- No schema, API-shape, or template change. No cache layout migration: existing cached
  directories already carry the zero-padded names and are read as-is.

## Out of scope

- Renaming chapter directories to a wider padding. That would be a cache migration for a
  sort problem that a comparator solves, and it would still break at the next width.
- Including the cover in the archive. Content-hash cover names have no slug mapping; doing
  it properly needs a store lookup and belongs in its own change.
