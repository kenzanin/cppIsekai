---
type: decision
title: Detail page design + enrichment plugin (alt cover/alt info)
description: "Detail page: cover/synopsis/artist/author/tags/chapters + hide-cover (blur/dim) + alt cover & alt info via separate enrichment plugins (mangadex/mangaupdates); lazy fetch cached in title_meta, persisted selection, auto-match with candidates"
tags: [detail, enrichment, plugin]
generated: { by: agent/mcp, at: "2026-11-06T14:31:16Z" }
status: stable
---

Manga detail page + enrichment plugin design, locked by discussion (no implementation yet; user drives staged openspec per page).

## Detail page contents
Cover, synopsis, artist, author, tags/genres, title, chapter list.

## Cover controls
- Hide cover: cover becomes blurred + dimmed (toggle button).
- Alt cover: button calls the ENRICHMENT plugin for available alt covers; clicking one swaps the displayed cover.
- Alt info: same enrichment path for alt title, synopsis, genre, artist, author; selectable like covers.

## Enrichment plugins (separate from website plugins)
- Sources first: mangadex, mangaupdates.
- Fetch timing: LAZY on first detail open, results CACHED to DB so offline reopen works and repeat opens don't refetch.
- Persistence: user's chosen cover + chosen info variant are PERSISTED per title (survives reopen).
- Title matching: auto-match by title+author, but when uncertain present CANDIDATE list for the user to pick.
- Storage: new table title_meta (title_id, source, cover_url, alt_title, synopsis, author, artist, tags, selected flag) -- all variants retained; selected flag marks the persisted choice.

## Open questions resolved
- Relationship to website plugins: enrichment plugins implement a different capability set (metadata search/lookup) than scraper plugins (chapter lists). Both are plugins orchestrated by the host.
- Chapter list itself comes from the WEBSITE plugin for the title's source site; enrichment only supplies metadata/alt data.

Relation: extends architecture/library-page (same title entity); consumer of the plugin system in manga-viewer-core.

# Related Concepts
- [Library page design (status, progress, done/new badges)](library-page.md): Detail page extends the library title entity with metadata and enrichment
- [cppIsekai core architecture: plugin-per-site manga viewer](manga-viewer-core.md): Enrichment plugins are a second capability family of the plugin system
