# mangadex-plugin Specification

## Purpose

Defines the observable behavior of the first real plugin — mangadex — serving as the reference implementation of the plugin ABI and the normalization contract.

## Requirements

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

### Requirement: Optional authentication
When the plugin folder carries a `config.json` with personal-client credentials
(`client_id`, `client_secret`, `username`, `password`), the plugin SHALL authenticate
against `https://auth.mangadex.org/realms/mangadex/protocol/openid-connect/token` using the
`password` flow — a form-urlencoded POST via `http.fetch` — and keep the resulting
`access_token` and `refresh_token` in `storage`.

#### Scenario: Login with credentials
- **WHEN** valid credentials exist and no token is stored yet
- **THEN** the plugin performs one password-flow POST and stores access/refresh tokens with an `expires_at` unix timestamp

### Requirement: Token refresh
The plugin SHALL refresh the access token via the `refresh_token` flow when the stored
`expires_at` (unix seconds) has passed or the API answers 401 (single retry, then error).

#### Scenario: Refresh on expiry
- **WHEN** the stored access token is past `expires_at`
- **THEN** the next API call first performs one refresh POST and succeeds without surfacing an error to the caller

#### Scenario: 401 mid-session
- **WHEN** an API call returns 401 while the stored token looks unexpired
- **THEN** the plugin refreshes once, retries the request, and only then returns the error shape if it still fails

### Requirement: Auth header scoping and graceful fallback
The plugin SHALL attach `Authorization: Bearer <token>` only to `api.mangadex.org`
requests — never to image/upload domains. Without `config.json`, the plugin SHALL run
unauthenticated exactly as before. Authentication failures SHALL surface as the standard
`{"ok": false, "error": ...}` shape, never a crash or retry loop.

#### Scenario: No credentials
- **WHEN** the plugin folder has no `config.json`
- **THEN** search/detail/chapters behave exactly as the unauthenticated spec requires, with no auth attempts and no errors

#### Scenario: Bad credentials
- **WHEN** the token endpoint rejects the login
- **THEN** the plugin returns the error shape with a message identifying auth failure and does not retry in a loop

#### Scenario: Authenticates and refreshes
- **WHEN** valid credentials exist and the stored access token is past `expires_at`
- **THEN** the next API call first performs one refresh POST and succeeds without surfacing an error to the caller

#### Scenario: 401 mid-session
- **WHEN** an API call returns 401 while the stored token looks unexpired
- **THEN** the plugin refreshes once, retries the request, and only then returns the error shape if it still fails

#### Scenario: No credentials
- **WHEN** the plugin folder has no `config.json`
- **THEN** search/detail/chapters behave exactly as the unauthenticated spec requires, with no auth attempts and no errors

#### Scenario: Bad credentials
- **WHEN** the token endpoint rejects the login
- **THEN** the plugin returns the error shape with a message identifying auth failure and does not retry in a loop

### Requirement: Mangadex pages
The plugin SHALL support a `pages` method: given `params.id` (chapter uuid) it SHALL return
`{"ok": true, "data": [<absolute image url>, ...]}` in reading order, sourced from the
at-home server endpoint for that chapter.

#### Scenario: Page urls in order
- **WHEN** the plugin is called with `method: "pages"` and a known chapter id
- **THEN** the data array is non-empty, entries are absolute https URLs under the resolved
  uploads host, and their count matches the chapter's page count

### Requirement: Mangadex genres
The plugin SHALL support a `genres` method returning
`{"ok": true, "data": [<English tag name>, ...]}` sourced from MangaDex's tag list.

#### Scenario: Genre list
- **WHEN** the plugin is called with `method: "genres"`
- **THEN** the response contains the MangaDex tag names (e.g. includes `Isekai`, `Fantasy`) and no request params are required
