# Spec Delta: detail-enrichment-fields

## MODIFIED Requirements

### Requirement: Persistence (title_meta)
The host SHALL persist enrichment state in a new table `title_meta` keyed by
`(plugin, slug)` with columns `fetched` (JSON array, one raw payload per source
including that source's fetch watermark) and `chosen` (JSON object
`{ title?, synopsis?, artist?, author?, cover?, tags_add[], tags_hidden[] }`).
`fetched` SHALL NOT expire (no TTL). `chosen` SHALL be overwritten in full on
save. Enrichment fetches and `chosen` saves SHALL NEVER write `saved_manga` or
`api_cache`: candidates stay in `title_meta`, and the raw plugin envelope stays in
`api_cache`. The one exception SHALL be the detail resolver materializing its
resolved, user-visible values (name, cover, synopsis, status, authors, artists, tags)
onto a saved row, as required by `library-save` — that write is the merge result, never
a raw candidate.

#### Scenario: Storage never clobbers source cache
- **WHEN** any enrichment fetch or chosen save executes
- **THEN** `saved_manga` and `api_cache` byte-contents are unchanged

#### Scenario: Resolved detail materializes on the row
- **WHEN** the detail resolver runs for a saved manga and the merged values differ from the stored ones
- **THEN** the resolved display fields are written to the `saved_manga` row, and `title_meta` keeps the candidates and `chosen`
