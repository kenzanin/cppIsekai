---
type: decision
title: "Normalisation contract: MangaDex canonical + chapter release_at"
description: "Host accepts only unix-time integers; JSONB hybrid policy probed on libsql 3.47: jsonb_* family present, filtered data stays columns, variant metadata in slim JSON columns"
tags: [normalization, json, schema]
generated: { by: agent/mcp, at: "2026-11-06T14:47:44Z" }
status: stable
---

Unix time + JSON storage policy, locked after a live probe of our vendored libsql amalgamation (no implementation yet).

## Unix time contract (extends normalization-contract)
The host accepts datetimes ONLY as INTEGER unix time (epoch seconds). Plugins convert site-specific date strings before handing data over. Templates render to human format. release_at on chapters, update timestamps, everything.

## JSON storage policy (hybrid, probed)
Probe results (C program against third_party/libsql/sqlite/sqlite3.c, libversion 3.47.0, linked with -lm):
- REAL JSONB support: jsonb_* function family present (jsonb_extract, jsonb_set, jsonb_patch, jsonb_insert, jsonb_remove, jsonb_replace, jsonb_object, jsonb_array, jsonb_group_array, jsonb_group_object) -- SQLite 3.45+ binary JSONB internal format, not just text json_*.
- json_extract / json_each / json_set all work; expression indexes over json_extract CAN be created but equality/containment filters still SCAN (EXPLAIN QUERY PLAN verified) -- no transparent indexing of JSON internals.
- Note: building the amalgamation standalone needs -lm (libsql vector/diskann code uses sqrt) -- our CMake target already links it via other libs; if the sqlite3 target ever stands alone, add m.

Policy:
1. FILTERED/INDEXED data stays real columns: status, total, read, has_update, plugin_id, release_at (unix int). These feed sidebar filters, FTS5, sorting, dedup. JSON here would force full scans.
2. DESCRIPTIVE/variant metadata goes JSON columns: title_meta becomes slim (title_id, source, selected, data JSON) holding alt titles, synopsis variants, raw enrichment payload per source. Flexible across sources; glaze (de)serializes host-side.
3. No per-source metadata tables. Table set: plugin, title, chapter, title_meta.

# Related Concepts
- [Detail page design + enrichment plugin (alt cover/alt info)](detail-page.md): Unix-time + JSON policy refines the normalisation contract concept (okf_update wipes relations)
- [cppIsekai core architecture: plugin-per-site manga viewer](manga-viewer-core.md): Normalisation duty belongs to plugins orchestrated by the host
- [Search & duplicate detection: FTS5 + BM25](fts5-search-dedup.md): Policy normalisasi unix-time + JSON menajamkan kontrak yang dieksekusi plugin
