---
type: decision
title: "A chapters parse is the whole list: upsert prunes upstream-gone identities"
description: "chapters_upsert now deletes chapter rows whose ids are absent from the incoming parse (both the ephem map and SQLite), because aggregator plugins fold duplicate releases into one id and the leftovers would linger. comik additionally merges its per-source rows by chapter number at parse time."
tags: [store, chapters, prune, comik, plugin, decision]
generated: { by: agent/mcp, at: "2026-11-27T13:19:23Z" }
status: stable
---

# Decision

Every `chapters` parse is authoritative for the complete list, so
`Store::chapters_upsert` deletes identities that the parse did not carry:

- ephem branch: `std::erase_if` over the pair map, dropping ids the parse did not carry
- SQLite branch: same transaction, `remove_all<ChapterEntity>(plugin && slug && not_in(chapter_id, keep))`

# Why

Plugins that fold duplicate upstream releases into one row (comik groups 5-10
scanlation copies of "Chapter 51" into a single entry) change the identity set
between parses. Without the prune, the dropped ids stayed in the row list and
the reading list showed the same chapter several times over.

# Consequences

- Flags live on the row, so pruning drops read/hidden bits for chapters the
  source no longer lists -- correct: the id is gone upstream too.
- The store test "upsert preserves flags; identity key wins; vanished ids
  pruned" now asserts `chapter_flags_get(b2)` is nullopt after b2 leaves the
  parse (was 0 before).
- A parse that fails mid-flight aborts the whole transaction (TxGuard), so a
  transient upstream error can never wipe the list.

# comik parser side

`plugins_dev/lua/comik/chapters.lua` merges by chapter *number* (first
occurrence wins, ids with unknown number 0 stay separate), parses
`Vol.0 Ch.30` labels with a `[Cc]h...` anchor (the volume digit is not the
number), and orders same-number rows by earliest release. 673 duplicated rows
became 60-61 distinct chapters.

# Related Concepts
- [Chapter state: one flags bitfield per chapter row, chapters as metadata hub](../architecture/chapter-state-flags.md): related
- [comik search ignored ?page -- every UI page returned the same 48 cards](../findings/comik-search-pagination-2026-11-25.md): related
