# Spec Delta

## Purpose

Keep the library's followed titles fresh without bombarding scanlation sources — a bounded, scheduled chapter-list refresh that only touches barely-read titles, capped, paced, and fully auditable.

## ADDED Requirements

### Requirement: Interval knob governs the schedule
The daemon SHALL read `[update] interval_days` (integer, minimum 1, default 1), `[update] per_plugin_limit` (integer, minimum 1, default 10) and `[update] request_delay_ms` (integer, minimum 0, default 1000) from the registry-backed config. A background scheduler SHALL run on each elapsed interval plus a randomized jitter of up to 15 minutes. All three knobs SHALL be editable from `/settings` (registry-backed; no restart required).

#### Scenario: Default interval is one day
- **WHEN** the ini sets no `[update]` section
- **THEN** the scheduler runs once every 24 hours (plus jitter)

#### Scenario: Invalid interval rejected
- **WHEN** the ini sets `interval_days = 0` or a non-integer
- **THEN** boot or config save reports the validation error and the value is not applied

### Requirement: Only barely-read titles participate
Auto-update SHALL consider only saved manga whose `read_ch` (chapters fully read at last rollup) is less than or equal to 1. Titles never opened (`read_ch = 0`) or with exactly one chapter read are included; titles with more are excluded.

#### Scenario: Barely-read title is a candidate
- **WHEN** a saved manga has `read_ch = 1` and no prior attempt this interval
- **THEN** it is eligible for the next batch

#### Scenario: Well-read title is excluded
- **WHEN** a saved manga has `read_ch = 5`
- **THEN** the scheduler never selects it

### Requirement: Batch cap per plugin per run
Each scheduled run SHALL select at most `[update] per_plugin_limit` (integer, minimum 1, default 10) eligible titles **per plugin**, ordered by oldest `update_attempts.last_attempt_at` first (never-attempted titles first). The cap is per-plugin, not global, so each site is bounded independently.

#### Scenario: Cap respected per plugin with many eligible titles
- **WHEN** 30 barely-read titles across 2 plugins are due and `per_plugin_limit = 10`
- **THEN** the run checks up to 10 titles per plugin (20 max) and the remainder wait for later runs

#### Scenario: Small plugin not starved by large one
- **WHEN** plugin A has 30 eligible titles and plugin B has 1, `per_plugin_limit = 10`
- **THEN** plugin B's title is checked in the same run; A is capped at 10

#### Scenario: Rotation prefers stale titles
- **WHEN** eligible titles exist and some have never been attempted
- **THEN** never-attempted titles are preferred; ties break by oldest attempt

### Requirement: Attempt tracking in the database
The store SHALL maintain an `update_attempts` table keyed by `(plugin, slug)` recording `last_attempt_at`, `outcome` (`ok` | `error` | `skipped`), and a short `error` string. Both the scheduler and the manual refresh path SHALL write/refresh this row.

#### Scenario: Successful auto-update recorded
- **WHEN** the scheduler refreshes a title's chapter list successfully
- **THEN** `update_attempts` holds `outcome = ok` for `(plugin, slug)` with `last_attempt_at = now`

#### Scenario: Failed attempt recorded
- **WHEN** the plugin call fails or the envelope fails validation
- **THEN** `update_attempts` holds `outcome = error` with the plugin/host error message

#### Scenario: Manual refresh counts as an attempt
- **WHEN** a user clicks the detail-page refresh button
- **THEN** the `update_attempts` row is stamped, and the auto-update scheduler skips that title for one interval

### Requirement: Pacing and backoff
The scheduler SHALL pace requests between titles by `[update] request_delay_ms` (integer milliseconds, minimum 0, default 1000). The knob SHALL be editable from `/settings` (registry-backed; read per tick, no restart required). On HTTP 429 (or repeated consecutive errors for a title), the title SHALL be skipped with increasing backoff (it becomes ineligible until its backoff window elapses); on backoff expiry the title rejoins the normal rotation.

#### Scenario: Pacing delay configurable
- **WHEN** the user sets `update.request_delay_ms = 2500` on `/settings`
- **THEN** subsequent runs wait 2500 ms between title refreshes

#### Scenario: Pacing zero allowed
- **WHEN** the user sets `update.request_delay_ms = 0`
- **THEN** the run proceeds without inter-title delay (user's explicit choice; backoff still applies)

#### Scenario: Rate-limited title backs off
- **WHEN** a plugin answers 429 for a title
- **THEN** the attempt is recorded as error and the title is skipped for N intervals (N grows per consecutive failure), without blocking the other 9 batch slots

#### Scenario: Manual path ignores backoff
- **WHEN** the user clicks refresh on a backed-off title
- **THEN** the manual refresh runs anyway and stamps a new attempt row

### Requirement: Update scope is chapter lists only
An auto-update attempt SHALL call only the plugin's `chapters` method (not `detail`, not covers). It SHALL NOT write `saved_manga` rows directly; any stat refresh flows through the shared refresh path's existing logic.

#### Scenario: Metadata not refetched
- **WHEN** an auto-update runs for a title
- **THEN** only the chapters envelope is requested; the title's stored name/cover/synopsis are unchanged by the auto-update itself

### Requirement: Observability and control
The settings page SHALL expose the interval knob, and the refresh endpoint's responses SHALL distinguish whether a title is auto-update eligible. Logs SHALL record each run (selected count, outcome counts). There is no per-title auto-update opt-out in this change; the global interval is the control.

#### Scenario: Run logged
- **WHEN** a scheduled run completes
- **THEN** a log line reports the run outcome (checked/ok/error/skipped counts) at info level

#### Scenario: Interval editable from settings
- **WHEN** the user edits `update.interval_days` on `/settings`
- **THEN** the value is saved via the existing merge-save and the scheduler honors the new interval on the next tick (read per tick, no restart required)

#### Scenario: Per-plugin limit editable from settings
- **WHEN** the user edits `update.per_plugin_limit` on `/settings`
- **THEN** the value is saved via the existing merge-save and the next run honors the new cap
