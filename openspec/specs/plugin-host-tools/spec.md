# plugin-host-tools Specification

## Purpose

Defines the host-tool functions plugins may call — network fetch with browser TLS fingerprinting, logging, and time — as one stable contract that every future runtime binding implements identically.

## Requirements

### Requirement: HTTP fetch with browser fingerprint
Plugins SHALL call `http.fetch(url, options)` to perform an HTTP request through the host's curl-impersonate client. `options` MAY contain `timeout_ms` and `profile`; when absent or empty, the host SHALL use the global defaults from `data/cppisekai.ini` (`fetch_timeout_ms`, `fetch_profile`). The result SHALL be a table with `status` (HTTP code), `body` (response body as text), and `headers`.

#### Scenario: Fetch with defaults
- **WHEN** a plugin calls `http.fetch("https://api.example.com/manga")` with no options
- **THEN** the request uses the `.ini`-configured timeout and TLS profile and returns `status`, `body`, and `headers`

#### Scenario: Per-call override
- **WHEN** a plugin calls `http.fetch(url, {timeout_ms = 5000, profile = "chrome116"})`
- **THEN** that single request uses 5000 ms and the chrome116 profile, leaving the global defaults unchanged

#### Scenario: Network failure
- **WHEN** the target is unreachable or times out
- **THEN** `http.fetch` returns a failure the plugin can detect, and the host logs the URL and the error

### Requirement: Logging from plugins
Plugins SHALL call `log.info(message)` and `log.error(message)`; messages SHALL appear in the host log pipeline (stdout and the websocket log page) tagged with the plugin id.

#### Scenario: Plugin logs its progress
- **WHEN** a plugin calls `log.info("fetched 20 titles")`
- **THEN** the message appears in the host log associated with that plugin

### Requirement: Time access
Plugins SHALL call `now()` to get the current time as INTEGER unix seconds, matching the host-wide unix-time contract.

#### Scenario: Plugin stamps a fetch
- **WHEN** a plugin calls `now()` and includes the value in its result
- **THEN** the value is unix seconds, consistent with host-generated timestamps
