# Spec Delta

## Purpose

Defines the observable behavior of the first real plugin — mangadex — serving as the reference implementation of the plugin ABI and the normalization contract.

## ADDED Requirements

### Requirement: Mangadex search
The mangadex plugin SHALL support the `search` capability: given `params.title`, it SHALL return `{"ok": true, "data": {"items": [...]}}` where each item carries MangaDex-canonical fields — `id`, `name`, `status` (one of ongoing/completed/hiatus/cancelled), `tags` (array of MangaDex tag names), `year`, and `cover_url`.

#### Scenario: Search returns canonical items
- **WHEN** the plugin is called with `method: "search"`, `params: {"title": "solo leveling"}`
- **THEN** the response is ok-shaped and every item's `status` and `tags` use MangaDex-canonical vocabulary (no site-specific taxonomy)

#### Scenario: No results
- **WHEN** the search matches nothing
- **THEN** the plugin returns `{"ok": true, "data": {"items": []}}` — an empty success, not an error

### Requirement: Mangadex detail and chapters
The plugin SHALL support `detail` (params: `id`) returning synopsis, author, artist, alt titles, tags, status, and cover URL, and `chapters` (params: `id`) returning chapter items with `number`, `title`, and `release_at` as INTEGER unix seconds — sourced from MangaDex's publish-at values.

#### Scenario: Detail fields arrive canonical
- **WHEN** the plugin is called with `method: "detail"` for a known id
- **THEN** the ok-shaped data includes synopsis, author, artist, alt titles, tags, status, and `cover_url`, all already in MangaDex format (MangaDex is the canonical source, so no translation is needed)

#### Scenario: Chapter timestamps are unix
- **WHEN** the plugin is called with `method: "chapters"` for a known id
- **THEN** every chapter's `release_at` is an INTEGER (unix seconds), never a date string

### Requirement: Network behavior
The plugin SHALL use `http.fetch` for all network access (never direct Lua sockets) and SHALL tolerate MangaDex API errors by returning the error shape `{"ok": false, "error": ...}` rather than crashing.

#### Scenario: Upstream API failure
- **WHEN** MangaDex returns a 5xx or the fetch fails
- **THEN** the plugin returns the error shape with a meaningful message and the host logs it against the plugin

### Requirement: Plugin identity assets
The plugin folder SHALL carry a logo file, and the host SHALL serve it at `/plugin/mangadex/logo` with the correct content type; wherever the plugin name renders in the UI (sidebar, filters), the logo SHALL appear with the name.

#### Scenario: Logo served and rendered
- **WHEN** a browser requests `/plugin/mangadex/logo`
- **THEN** the image bytes are returned with the matching content type, and the sidebar's plugin list shows the logo next to the mangadex entry
