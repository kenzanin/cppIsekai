# detail-enrichment-fields (delta)

## ADDED Requirements

### Requirement: Recommendation cap
Enrichment plugins SHALL cap returned `recommendations` items at **10** each (MangaDex, MangaUpdates). Cover candidate caps SHALL remain 5.

#### Scenario: Ten recommendations returned
- **WHEN** MangaDex or MangaUpdates returns more than 10 scored recommendations
- **THEN** the envelope carries at most 10 items, ordered by score/weight descending

### Requirement: Related-reading selection in the detail wizard
The Fetch-detail wizard SHALL render a read-only "related readings" segment listing the merged recommendations from all enrichment sources that returned a `recommendations` payload for this manga. The list SHALL dedupe identical items across sources (MangaDex wins on normalized `url||id||name` collision). The segment SHALL offer **Add all** and **Remove all** buttons and no per-item controls.

#### Scenario: Add all appends to the detail page
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

## ADDED Requirements

### Requirement: Related section merge
The detail page Related section SHALL render the union of (a) the scraper plugin's `related` method output (unchanged behavior, including atl seam gating) and (b) `chosen.related_add`, deduplicated by normalized `url||id||name` with first occurrence winning. Links SHALL remain external (`target=_blank rel=noopener`); no Library entries are created.

#### Scenario: Atsumaru shows wizard-added items only
- **WHEN** saved manga's plugin declares no `related` capability and the user taps Add all
- **THEN** the Related section shows exactly the merged wizard list; it is hidden again after Remove all
