# Design: cbz-chapter-order

## D1. Numeric sort key, not a wider padding

**Decision.** Keep the entry names and container as a flat sequence sorted by
`(chapter number, page number)`, parsed from the directory and file names.

**Alternatives.**
- *Widen the padding to four digits and keep the map.* Fixes this case and breaks at
  `ch10000`; the correctness of the archive would depend on an unrelated cache-layout
  constant that a plugin with a large chapter count can violate. Rejected.
- *Keep `std::map` but zero-pad the key only in the comparator.* Works, but then the sort
  key and the stored key diverge, and the comparator silently re-encodes layout knowledge.
  A parsed numeric key is the same amount of code and says what it means.

**Consequence.** Entry names remain byte-identical to today, so no cache migration and no
consumer sees a different filename. Only the order inside the archive changes.

## D2. Parse defensively, do not throw

Chapter directories are `ch<digits>` and pages are `p<digits>.<ext>`, but a stray entry in
the cache tree must not take the export route down. Names that do not parse fall back to a
large sort key derived from the string, so they sort last and deterministically instead of
aborting the request. This mirrors the route's existing tolerance: an empty or partially
populated cache yields an empty or partial archive rather than an error.

## D3. Regression test asserts position, not membership

`tests/test_export.cpp` already checks entry presence through substring search on the raw
zip bytes. Presence cannot catch this bug — both chapters were present all along. The new
case caches `ch999` and `ch1000`, exports, and compares the byte offsets of the two entry
names in the returned archive, which is the local-file-header order. Asserted offsets say
`ch999` first; the current implementation puts `ch1000` at offset 67 and `ch999` at 154.
