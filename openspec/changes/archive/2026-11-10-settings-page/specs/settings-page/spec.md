# Spec Delta: settings-page

## ADDED Requirements

### Requirement: Settings page lists every registry knob with provenance
`GET /settings` SHALL render one row per config registry knob (`section.key`), showing the doc text, the resolved value, and the layer that won (default | ini | env | flag). Knobs whose winning layer is `env` or `flag` SHALL render read-only with a hint naming the source (`CPPISEKAI_*` / `--flag`); ini- and default-backed knobs SHALL be editable.

#### Scenario: All knobs visible
- **WHEN** the user opens `/settings`
- **THEN** every registry knob (`server.port`, `server.proxy`, `plugin.*`, `cache.*`, `paths.*`, `log.level`) appears exactly once, grouped by section, with doc text and current value

#### Scenario: Env-owned knob is read-only
- **WHEN** a knob is set via its `CPPISEKAI_*` env var or an explicit CLI flag
- **THEN** its row renders read-only and a hint names the winning source instead of offering an ini edit that would not apply

### Requirement: Read projection endpoint
`GET /config` SHALL return a JSON projection of the registry: per knob `{section, key, fullKey, doc, type, flag, env, default, value, source}` where `value` is the resolved (default < ini < env < flag) value and `source` is the winning layer. It SHALL re-resolve on every request (no boot-time snapshot) and SHALL NOT mutate the ini.

#### Scenario: Projection reflects on-disk ini
- **WHEN** the ini is edited on disk and `GET /config` is called
- **THEN** the returned `value` reflects the new ini contents without a daemon restart

#### Scenario: Malformed ini falls back per layer
- **WHEN** a numeric knob holds an unparseable ini value
- **THEN** `GET /config` returns a well-formed response carrying the registry default for that knob (same lenient rule as boot resolve), never a 500

### Requirement: Edits go through the existing merge-save
The page SHALL submit changed fields only, as `{"section.key": string|number}` to `POST /config`. Unknown keys or bad numerics SHALL surface the server's error verbatim; comments and ordering in the ini SHALL survive the save.

#### Scenario: Successful save
- **WHEN** the user edits an ini-backed knob and submits
- **THEN** `POST /config` returns `{ok:true, written:n}`, the page re-reads `GET /config`, and rows show the resolved post-save values

#### Scenario: Rejected body
- **WHEN** a value fails server-side validation (unknown key, non-integer for an Int/Int64 knob)
- **THEN** no file is written and the page shows the server error on its `err` line; field values stay as the user typed them

#### Scenario: Comment preservation
- **WHEN** a knob with surrounding `; comment` lines is saved from the page
- **THEN** those comments and the key order in the ini are unchanged after the write

### Requirement: Restart and live hints
Rows whose effect needs a restart SHALL carry a "restart to apply" badge (`server.port`, `paths.library`, `paths.log_file`, `paths.data_dir`, `cache.cache_max_mb`, `cache.image_format`, `cache.cover_max_px`, `log.level`). `plugin.fetch_profile` SHALL be marked as applied live, matching the existing `plugins.set_default_profile` behavior on `POST /config`.

#### Scenario: Live profile edit
- **WHEN** the user changes `plugin.fetch_profile` and submits
- **THEN** the running Host picks up the new profile without a restart (no badge shown for this row)

#### Scenario: Restart row
- **WHEN** the user edits `server.port` and submits
- **THEN** the save succeeds and the row (and page) shows the restart badge until the daemon reboots

### Requirement: Navigation entry
The top navigation SHALL include `Settings` linking to `/settings`, placed after `Plugins`.

#### Scenario: Nav renders
- **WHEN** any page renders the top bar
- **THEN** a `Settings` link is present and marked active on `/settings`
