# detail-enrichment-fields Specification

## Purpose

Let a saved (in-library) manga's detail page preview, merge, and revert metadata
and covers fetched from dedicated enrichment plugins, without ever overwriting
the values the source plugin produced.

## Requirements

### Requirement: Enrichment plugin directory and contract
The host SHALL load enrichment plugins from `data/enrichment/<id>/` as a separate registry from scraper plugins, reusing the same Lua/sol2 runtime and host tools. Enrichment plugins SHALL declare capability `enrichment` and SHALL serve only an `enrich` ABI method.

#### Scenario: Enrichment plugin loaded from the enrichment directory
- **WHEN** the host boots with `data/enrichment/mangadex/main.lua` present
- **THEN** the plugin loads as an enrichment source and is not listed among scraper plugins

#### Scenario: Enrichment plugin cannot serve chapters
- **WHEN** an enrichment plugin's envelope carries `chapters` or `pages`
- **THEN** the host ignores those keys and only the enrichment contract is consumed

### Requirement: Two fetch paths
The detail page SHALL offer, for saved mangas only, two independent fetch paths (metadata vs covers), each with a normal and a force action. Normal SHALL serve stored `fetched` when present, else fetch once; force SHALL always hit the source website and overwrite `fetched`.

#### Scenario: Normal fetch uses the stored payload
- **WHEN** `title_meta.fetched` for `(plugin, slug)` is present and the user taps Fetch detail
- **THEN** the enrichment plugin is NOT called and the stored payload is returned

#### Scenario: Force fetch always hits the site
- **WHEN** the user taps the force action
- **THEN** the enrichment plugin is called regardless of stored state and `fetched` is replaced

#### Scenario: First-ever fetch
- **WHEN** no stored `fetched` payload exists
- **THEN** the normal tap calls the enrichment plugin once and stores the result

### Requirement: Candidates are user-selected, never auto-applied
The detail page SHALL render all fetched candidates from all enrichment sources
as one list ordered per source (MangaDex candidates grouped, then MangaUpdates,
etc.), with duplicates preserved. The user SHALL choose which candidate of each
field replaces the field displayed; no chosen value SHALL be applied without an
explicit user selection. Unsupported or missing fields in a source SHALL simply
omit candidates.

#### Scenario: Cross-source candidate list
- **WHEN** both enrichment plugins return candidates
- **THEN** both are listed, grouped by source, duplicates included, and nothing changes until the user picks

### Requirement: Field-level replace and tag merge
Non-tag chosen fields SHALL replace displayed source values; tags SHALL render as `(source ∪ chosen tags_add) − chosen tags_hidden`. Reset detail restores non-tag source values; `tags_hidden` hides individual tags without deleting payloads.

#### Scenario: Tag select then hide
- **WHEN** the user selects a fetched tag and later hides it
- **THEN** the tag is not rendered, while other selected tags still render

### Requirement: Cover wizard
Fetch covers SHALL open a wizard listing candidate cover URLs; choosing one SHALL
persist it as the displayed cover; the cover image SHALL be served through
`/cache/img` so repeated renders of the same URL hit disk, not the source website.

#### Scenario: Repeated taps on an already-seen cover
- **WHEN** the user taps fetch covers twice and one candidate was already cached
- **THEN** the cached image is served and no HTTP request reaches the source website for that URL

### Requirement: Reset cover and reset detail
The detail page SHALL offer **Reset cover** and **Reset detail**. Reset cover
SHALL remove the chosen cover override and restore the `saved_manga` cover URL.
Reset detail SHALL remove the chosen overrides for title, synopsis, artist,
author, tags_add and tags_hidden, and restore the source-plugin detail/chapters
state (`api_cache` payloads) and the `saved_manga` name. Reset SHALL NOT delete or
modify `fetched`; the wizard/fetcher can re-apply from the stored payload without
a network call.

#### Scenario: Reset after merge
- **WHEN** the user has chosen enrichment values and taps Reset detail
- **THEN** the displayed title/synopsis/artist/author and tags revert to source-plugin values and notes/watermark of fetched data remain available

### Requirement: Persistence (title_meta)
The host SHALL persist enrichment state in a new table `title_meta` keyed by
`(plugin, slug)` with columns `fetched` (JSON array, one raw payload per source
including that source's fetch watermark) and `chosen` (JSON object
`{ title?, synopsis?, artist?, author?, cover?, tags_add[], tags_hidden[] }`).
`fetched` SHALL NOT expire (no TTL). `chosen` SHALL be overwritten in full on
save. Original `saved_manga` and `api_cache` rows SHALL NEVER be written by
enrichment code paths.

#### Scenario: Storage never clobbers source cache
- **WHEN** any enrichment fetch or chosen save executes
- **THEN** `saved_manga` and `api_cache` byte-contents are unchanged

### Requirement: Recommendation cap
Enrichment plugins SHALL cap returned `recommendations` items at 10 each (MangaDex, MangaUpdates). Cover candidate caps SHALL remain 5.

#### Scenario: Ten recommendations returned
- **WHEN** MangaDex or MangaUpdates returns 10 scored recommendations
- **THEN** the envelope carries at most 10 items, ordered score/weight descending

### Requirement: Related-reading selection in detail wizard
The Fetch-detail wizard SHALL render a read-only "related readings" segment listing the merged recommendations all enrichment sources returned for the manga. The list SHALL dedupe identical items across sources (MangaDex wins on normalized `url||id||name` collision). The segment SHALL offer **Add all** / **Remove all** buttons and no per-item controls.

#### Scenario: Add all appends to detail page
- **WHEN** the user taps Add all in the wizard
- **THEN** `title_meta.chosen.related_add` is overwritten with the full merged wizard list, and the detail page's Related section renders scraper related items appended with `related_add`, duplicates removed (scraper item wins)

#### Scenario: Remove all clears only wizard-sourced items
- **WHEN** the user taps Remove all
- **THEN** `title_meta.chosen.related_add` is removed; scraper-plugin related items, if any, still render; the wizard list itself remains visible

#### Scenario: MangaDex wins on duplicate
- **WHEN** MangaDex and MangaUpdates recommend the same work (normalized url/id/name match)
- **THEN** only the MangaDex entry is shown in the wizard and stored in `related_add`

### Requirement: Related-reading persistence
`chosen.related_add` SHALL be an array of `{name, url, id?}` (`name`/`url` required non-empty strings). It SHALL ride the existing `POST /api/enrichment/chosen` full-overwrite with the same key whitelist and validation strictness as other chosen keys. `saved_manga` and `api_cache` SHALL NOT be written. `Reset detail` SHALL clear `related_add` together with the other non-tag chosen fields.

#### Scenario: Invalid related_add rejected
- **WHEN** a chosen payload carries `related_add` with an entry missing `name` or `url`
- **THEN** the POST returns 400 and `chosen` is left unchanged

#### Scenario: Reset detail clears related_add
- **WHEN** the user taps Reset detail
- **THEN** the detail page's Related section shows only scraper-plugin related items (if any); `fetched` remains intact

### Requirement: Related section merge
The detail page Related section SHALL render the union of (a) the scraper plugin's `related` method output (unchanged behavior, including the caps gating) and (b) `chosen.related_add`, deduplicated by normalized `url||id||name` with first occurrence winning. Links SHALL remain external (`target=_blank rel=noopener`); no Library entries are created.

#### Scenario: Atsumaru shows wizard-added items only
- **WHEN** a saved manga's plugin declares no `related` capability and the user taps Add all
- **THEN** the Related section shows exactly the merged wizard list; it is hidden again after Remove all
