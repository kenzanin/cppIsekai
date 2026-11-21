# Spec Delta

## Purpose

Defines one transient feedback contract for every user-initiated operation in the web UI, so success, failure and pending states are always visible.

## ADDED Requirements

### Requirement: Transient action feedback

Every user-initiated operation that calls the server SHALL emit exactly one transient
toast reporting success or failure. The host SHALL expose `toast(kind, message)` with
`kind` in `success`, `error`, `info`, auto-dismiss after 5 seconds (8 for errors), and
a maximum of 4 simultaneous toasts. Local-only state changes SHALL NOT emit toasts.

#### Scenario: Successful update
- **WHEN** a user clicks Update on a manga detail page and `/api/refresh` returns `ok: true`
- **THEN** an emerald success toast appears and the previous error line is cleared

#### Scenario: Failed update
- **WHEN** `/api/refresh` returns `ok: false` with an error string
- **THEN** a red error toast containing that string appears and the button re-enables

#### Scenario: Local filter toggle
- **WHEN** a user toggles a grid filter
- **THEN** the filter's active state changes and no toast is shown

### Requirement: Pending and result indicators

Every wired button SHALL show a pending indicator while its request is in flight and
a result LED for 3 seconds afterward. The button SHALL be disabled while pending and
re-enabled in both the success and failure paths. A thrown exception SHALL be converted
to an error toast rather than an unhandled rejection.

#### Scenario: Pending state
- **WHEN** a save request is in flight
- **THEN** the Save button is disabled and its LED is amber

#### Scenario: Exception during request
- **WHEN** `fetch` throws (network failure)
- **THEN** an error toast appears, the LED turns red for 3 seconds, and the button is re-enabled

### Requirement: Shared implementation surface

The feedback primitive SHALL be defined once in a shared mustache partial included by
both the common page shell and the reader page shell. No new JavaScript dependency,
static route, or server response field SHALL be introduced.

#### Scenario: Partial reused
- **WHEN** any page that includes the common shell or the reader shell calls `toast('success', 'done')`
- **THEN** the same toast host and styling are used

#### Scenario: No server change
- **WHEN** a wired handler receives `{ok: false, error: 'x'}`
- **THEN** the UI reports it without any new response field from the server
