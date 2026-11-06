---
type: decision
title: "Search & duplicate detection: FTS5 + BM25"
description: Manga search + duplicate detection via FTS5 + BM25 on the libsql amalgamation; replaces LIKE search; duplicate candidates ranked by BM25 similarity
tags: [search, fts5, dedup]
generated: { by: agent/mcp, at: "2026-11-06T14:37:08Z" }
status: stable
---

Search + duplicate detection decision (no implementation yet; user drives staged openspec per page).

## Decision
- Search across manga titles uses SQLite FTS5 (full-text search) with BM25 ranking (bm25() ranking function built into FTS5).
- Duplicate detection also rides FTS5: same index, high BM25 similarity candidates for dedup review.

## Why FTS5 over current LIKE
- Current search is LIKE '%q%' with manual escaping (like_pattern + .escape) -- substring match, no relevance, no tokenization. Fine for 48 seed rows, weak for a real library.
- FTS5 gives: tokenized matching, prefix queries (col:term*), bm25() ranked results, and it is ALREADY IN the libsql amalgamation we compile (fts5 present in sqlite3.c; verify with CREATE VIRTUAL TABLE at implementation time -- amalgamation must be compiled with SQLITE_ENABLE_FTS5 or include fts5.c; probe first).

## Expected shape at implementation
- FTS5 virtual table over title fields (name + alt titles + maybe synopsis from title_meta), kept in sync via triggers or store methods.
- Search route switches from LIKE to MATCH + ORDER BY bm25(title_fts).
- Duplicate detection: query new title's name against the index, threshold on bm25 score -> duplicate candidates for review before import.
- Store remains the only SQL gate; FTS tables live in src/store/.

## Caveat to verify at implementation
- Amalgamation was vendored from libsql-ffi/bundled: confirm FTS5 module compiles in (probe: CREATE VIRTUAL TABLE ... USING fts5). If not, add fts5.c to the build or define SQLITE_ENABLE_FTS5.

# Related Concepts
- [Source layer map and rules (src/<scope>/, .mustache templates)](../references/code-layers.md): FTS5 search upgrades the Store search layer described in code-layers
- [Library page design (status, progress, done/new badges)](library-page.md): FTS5 tables are part of the library data model
