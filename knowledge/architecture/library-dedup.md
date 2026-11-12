---
type: Fact
title: "library-dedup: signal columns on saved_manga; site plugins carry description, not synopsis"
description: "library-dedup ships columns synopsis/alt_titles/dup_group on saved_manga; source-envelope sync reads synopsis with 'description' fallback because mangadex/atsumaru detail envelopes expose only 'description'; alt_titles appears only in enrichment plugins, stored verbatim when present in source envelope."
tags: [library, dedup, store, fts5, plugin-envelope]
generated: { by: agent/cli, at: "2026-11-12T00:59:30Z" }
status: draft
---

Shipped 2026-11-12 (openspec library-dedup 11/11, 16/16 ctest). saved_manga += synopsis TEXT '', alt_titles TEXT '' (JSON array verbatim), dup_group INTEGER 0; boot pragma.table_info + ALTER TABLE probe. IMPORTANT (2026-11-12 correction): enrichment detail envelopes DO carry alt_titles and ARE now fed into saved_manga — earlier 'enrichment deliberately NOT used' note is obsolete per user ruling 'alt datang dari jalur enrichment plugin'. pipeline: enrichment fetch /api/enrichment/<id>/detail (force=1 ok) lands in enrich_internal.hpp enrichment_source_response AFTER title_meta_put_fetched: parses data.alt_titles (array of strings or {title,lang,main}), flattens titles, saved_signal_update(plugin,slug,{},json) (empty synopsis = untouched) + recompute_dup_groups(). Enrichment MU/MD alt_titles live in fetched envelope [{title,lang,main}], flatten with title only. Site plugins (mangadex/atsumaru) detail envelopes carry only 'description', no alt_titles. dup smoke 2026-11-12: 0 clusters in live library (no true duplicate pair; FTS MATCH path verified working against library_fts, e.g. '{name alt_titles} : (isekai OR maid)' returns rows). Live alt_titles populated 9/10 titles (1 upstream 502). auto-engaged sort=dup + Duplicates chip in grid (grid_body.mustache); completion: chip visible iff any card data-dup != 0. Live status casing differs per plugin (atsumaru 'Completed', mangadex 'ongoing') — complete-ish comparisons must be case-insensitive.

# Related Concepts
- [Library page design (status, progress, done/new badges)](library-page.md): dup_group/alt_titles fuel the duplicate chip, dup sort and cross-plugin dedup UX on library-page
- [library-grid-stats: saved_manga stat columns + envelope-derived sync](library-grid-stats.md): both land saved_manga columns and are synced from the same envelope envelope-derived sync pass
