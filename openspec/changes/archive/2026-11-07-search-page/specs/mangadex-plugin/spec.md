# mangadex-plugin delta

## ADDED Requirements

### Requirement: Mangadex genres
The plugin SHALL support a `genres` method returning
`{"ok": true, "data": [<English tag name>, ...]}` sourced from MangaDex's tag list.

#### Scenario: Genre list
- **WHEN** the plugin is called with `method: "genres"`
- **THEN** the response contains the MangaDex tag names (e.g. includes `Isekai`, `Fantasy`) and no request params are required

## MODIFIED Requirements

### Requirement: Mangadex search
The mangadex plugin SHALL support the `search` capability: given `params.title` (aliases
`query`, `q`), optional 1-based `page`, and optional `tags` (comma-joined English tag
names, AND semantics), it SHALL return
`{"ok": true, "data": {"items": [...], "total": <int>}}` where items carry the canonical
fields and `total` is MangaDex's total for the query. Results SHALL be ordered by
`followedCount desc` (site-ranking parity).

#### Scenario: Canonical items with total
- **WHEN** the plugin is called with `method: "search"`, `params: {"title": "one piece", "page": 1}`
- **THEN** the response is ok-shaped, `data.total` is an INTEGER >= items length, and the first item is the exact-title match per site ranking

#### Scenario: Search returns canonical items
- **WHEN** the plugin is called with `method: "search"` for any matching title
- **THEN** every item's `status` and `tags` use MangaDex-canonical vocabulary (no site-specific taxonomy)

#### Scenario: No results
- **WHEN** the search matches nothing
- **THEN** the plugin returns `{"ok": true, "data": {"items": [], "total": 0}}` — an empty success, not an error

#### Scenario: Page windows
- **WHEN** the same query is repeated with `page: 2`
- **THEN** a disjoint window of items is returned, clamped to the last page when out of range

#### Scenario: Tag AND filter
- **WHEN** `tags: "Isekai,Fantasy"` is passed
- **THEN** only manga carrying both tags return, and `total` reflects the filtered query
