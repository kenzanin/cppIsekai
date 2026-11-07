# mangadex-plugin delta

## ADDED Requirements

### Requirement: Mangadex pages
The plugin SHALL support a `pages` method: given `params.id` (chapter uuid) it SHALL return
`{"ok": true, "data": [<absolute image url>, ...]}` in reading order, sourced from the
at-home server endpoint for that chapter.

#### Scenario: Page urls in order
- **WHEN** the plugin is called with `method: "pages"` and a known chapter id
- **THEN** the data array is non-empty, entries are absolute https URLs under the resolved
  uploads host, and their count matches the chapter's page count
