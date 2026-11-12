## Context

`decisions/fts5-search-dedup` already locked FTS5 + BM25 as the duplicate mechanism (search page scope) and notes the amalgamation includes FTS5 (probe CREATE VIRTUAL TABLE first). User refined scope: duplicates = same work across titles AND synopses (all of them, both plugin-source), cross-plugin allowed, near-duplicates must sit adjacently in the grid when filtered. Persisting synopsis/alt-title is deferred to after library work (this change is the first consumer).

## Goals / Non-Goals

**Goals:** store synopsis + alt titles for saved entries; FTS5 index; dup clustering; grid filter clusters adjacently.

**Non-Goals:** wizard UI for synopsis/alt-title display (later change); cross-unsaved-library dedup; auto-merge/delete; read-parallel DB (deferred, see OKF `decisions/db-read-parallel`); FTS5 for the main search page (separate change).

## Decisions

**D1 — Signal columns on `saved_manga`.** `synopsis TEXT` (default ''), `alt_titles TEXT` (default '', JSON array of strings), `dup_group INTEGER DEFAULT 0`. From the plugin detail envelope on save/refresh/detail-open (source plugin only — NOT enrichment). `normalized` content is not stored; FTS5 holds tokenized text.

**D2 — FTS5 table, external-content or standalone?** Standalone `library_fts(name, alt_titles, synopsis)` keyed by `(plugin, slug)` — simplest; content is small (saved entries only). Sync: explicit `INSERT/DELETE` in the same store calls that write the row (`save_manga`, `unsave_manga`, `saved_stats_update` when synopsis/alt_titles change); no triggers (keeps schema portable, one code path). Probe `CREATE VIRTUAL TABLE … fts5` at store boot; on failure (amalgamation without FTS5) log warn and leave `dup_group=0` everywhere — grid chip hidden (controls chip already hides when no data).

**D3 — Clustering algorithm.** For each entry: tokenize normalized `name (unaccented, lowercased, stripped punctuation)` + alt_titles; `MATCH` top 20 candidates from FTS5; score Dice coefficient over title-token multisets AND synopsis token sets; similarity = max(title_dice, 0.5*title_dice + 0.5*synopsis_dice) >= 0.6 → link. Connected components → `dup_group` (reuse smallest existing component id or sequential; 0 when singleton). Recompute wholesale on refresh/save/unsave (O(N²) worst case on N=hundreds, fine). Runs on `db_pool` writes + read-only raw connection for the scan.

**D4 — Adjacent rendering in grid.** When the "duplicate" chip is active, the client sorts visible rows by `dup_group` (non-zero first, then name) so clusters sit together; the sort is applied client-side per controls D6 (add `dup` as a sort mode engaged automatically with the chip).

**D5 — Normalization.** unicode-aware lowercase + strip punctuation + collapse whitespace for titles; synopsis normalized the same minus stopword removal (keep simple); reuse `normAltTitle`-style logic host-side in C++ (no new dep; a small tokeniser function in store_search.cpp).

**D6 — Thresholds configurable?** NO config knob (YAGNI); constants in one header with ponytail note.

## Risks / Trade-offs


- False positives on short titles ("One Piece" vs "One Piece Party") — Dice on token multiset with length normalization mitigates; threshold 0.6 conservative; user reviews, no auto-merge.
- Storing synopsis duplicates rows that also exist in `title_meta` (enrichment) — different provenance; keep separate.
- FTS5 availability depends on amalgamation flags — probe and degrade to "no duplicates" (chip hidden), never crash.
