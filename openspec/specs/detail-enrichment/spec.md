# detail-enrichment Specification

## Purpose

Enrich the manga detail page with metadata returned by the plugin `detail` method: synopsis, authors, artists, plugin attribution (name + optional logo), and a stale-served Related reading section backed by the source site's recommendations.

## Requirements

### Requirement: Detail page renders enriched metadata
`GET /view/manga/<plugin>/<slug>` SHALL render the synopsis (`detail.description`), the author(s), and the artist(s) returned by the plugin `detail` method, in addition to the existing metadata (name, cover, status, tags, release date). Absent fields SHALL render nothing (no empty placeholders, no broken layout).

#### Scenario: Enriched detail renders
- **WHEN** the plugin returns `description`, `authors: ["Song Shu 200"]`, `artists: ["Someone Else"]`
- **THEN** the detail page shows the synopsis paragraph, an "Author" line listing the authors, and an "Artist" line listing the artists

#### Scenario: Missing fields degrade gracefully
- **WHEN** the plugin returns no description/authors/artists
- **THEN** the corresponding blocks are not rendered and the rest of the page is unchanged

### Requirement: Detail page shows plugin attribution
The detail page SHALL display the provider plugin's name and its logo (`/plugin/<id>/logo`), served only when the plugin has a logo file (otherwise the name alone, no broken image).

#### Scenario: Attribution renders with logo
- **WHEN** the plugin `mangadex` has a `logo.png` and the page loads
- **THEN** the detail page shows the plugin name and the logo image

#### Scenario: Missing logo falls back to name only
- **WHEN** the plugin has no logo file
- **THEN** the plugin name renders without an image element

### Requirement: Related reading section
`GET /api/related/<plugin>/<slug>` SHALL return a stale-served JSON list of related manga for the title (MangaDex `GET /manga/<id>/recommendation`, candidates resolved in one batched `/manga?ids[]=...&includes[]=cover_art` call, sorted by recommendation score descending). Each entry SHALL carry at least `id`, `name`, `cover`, and `url` (the title's page on the source site).

#### Scenario: Related list renders
- **WHEN** the detail page loads and the plugin returns recommendations
- **THEN** the Related section renders text-only title links, each opening the entry's `url` on the source site

#### Scenario: Recommendation endpoint failure
- **WHEN** the plugin `related` call fails (upstream error)
- **THEN** the section renders nothing (no error banner blocking the rest of the page) and the response indicates failure per the stale-serve contract

### Requirement: Plugin detail contract is additive
The plugin `detail` method SHALL extend the canonical manga envelope with `description`, `authors`, `artists`. Search, chapters, pages and the stale-cache envelope layout SHALL remain unchanged. js/wasm runtimes are out of scope for this change; the Lua plugin implements the new fields.

#### Scenario: Older plugins keep working
- **WHEN** a plugin returns a detail payload without `description`/`authors`/`artists`
- **THEN** the page renders as before (no errors)

#### Scenario: Recommendation score order
- **WHEN** the plugin `related` method returns entries with scores
- **THEN** the response preserves score-descending order
