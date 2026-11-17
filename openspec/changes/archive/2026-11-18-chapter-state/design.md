# Design: chapter-state

## D1 — Per-row flags column, not a global bitmap

`chapters.flags INTEGER` on every chapter row; `bit0=read`, `bit1=hidden`, `bit2+` reserved.
Mirrors `saved_manga.status_flags` (store.hpp:45). A new state costs one bit, no migration.
Rejected: bitmap over canonical chapter position — positions shift on retcon/insert and need a
persisted id→bit map, which is the table we already have, plus corruption on reorder.

## D2 — Identity keyed `(plugin, slug, chapter_id)`; rows persist

Chapter rows are created lazily on the first successful chapters parse and never pruned when
flags return to 0 (one upsert path, no delete path; ~100 rows/title is noise for sqlite).
`chapter_id` is the plugin's own id — stable across refreshes, duplicate numbers (24a/24b), ch0.
Legacy `host/skip/<id>` keys carry no manga identity, so inheritance happens inside
`chapters_upsert` at first insert of an id: the parse that claims the id is the attribution —
skip=1 → bit1, read ≥ total(>0) → bit0, existing rows ride through untouched. (A boot-time pass
was the first sketch; no rows exist at boot, since rows appear lazily on parse.) Re-claiming an
id under another title inherits nothing (insert-only). Accepted loss: read/skip state keyed by
ambiguous ids across titles.

## D3 — One generic bits endpoint, no per-state routes

`POST /chapter/flags {plugin, slug, ids?: [chapter_id], all?: true, set?: int, clear?: int}`
→ read-modify-write `flags = (flags | set) & ~clear` per row, one `OpTrace` scope (#5463).
Read state = `set:1` / clear = `set:0,clear:1`; future hidden = bit1. The range semantics
("1..N") stay client-side: the detail page already holds the chapter list, so it resolves ids
and posts once — the server never enumerates chapters (Suwayomi `updateChapters(ids, patch)` parity).

## D4 — chapter_pages replaces on parse; fs stays the binary source of truth

`chapter_pages(chapter_id FK→chapters.id? no — composite (plugin,slug,chapter_id) natural key,
idx, url)`, child rows deleted+reinserted per successful pages parse (replace-all, one txn).
Image cache paths stay derived from the filesystem layout; the DB never stores file paths —
`data/cache` boot-scan remains the image index. `chapter_pages` consumer in this change: the
completion threshold for auto-read (COUNT per chapter); queryable source-url history is the
stated goal (`api_cache` envelopes remain the serving layer, untouched).

## D5 — `read` bit is the single read truth; completion sets it host-side

`/chapter/<id>/read` (progress bump) sets bit0 when `page >= COUNT(chapter_pages)` for the
chapter. The detail check glyph lit = bit0 (was `pages_read >= total` client derive — same
visible outcome for new activity). `/chapter/state` returns `read: true|false` (bit) and keeps
`progress` (pages-read count, legacy key) for the sub-label; `read_ch` rollup on `saved_manga`
counts bit0 rows. Rejected: keeping the client-side derive — "clear all read status" must be
able to turn the check off, which a progress-derive cannot express.

## D6 — Consumers read flags through one store helper

`chapter_flags_get(plugin, slug, chapter_id)` / `_list(plugin, slug)`; reader window skip
filter (`http_api_reader.cpp`, worker mirror) and `/chapter/state` switch to it in the same
change — no dual-read period. Legacy-key inheritance happens inside `chapters_upsert` (D2); no
boot pass exists.
