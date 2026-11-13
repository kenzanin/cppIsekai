# fetch-gate delta

## MODIFIED Requirements

### Requirement: HTTP fetch with browser fingerprint
Plugins SHALL call `http.fetch(url, options)` to perform an HTTP request through the host's
curl-impersonate client. `options` MAY contain `timeout_ms`, `profile`, `method` (default
`"GET"`), `body` (request body string), and `headers` (string-keyed table of request
headers, applied after built-ins so plugins can override Accept/User-Agent). When
`timeout_ms`/`profile` are absent, the host SHALL use the global defaults from
`data/cppisekai.ini`. The result SHALL be a table with `status` (HTTP code), `body`
(response body as text), and `headers`.

#### Scenario: Chrome-family profile served by impersonate lib
- **WHEN** a plugin calls `http.fetch(url, {profile = "chrome116"})`
- **THEN** the request is served by the linked `libcurl-impersonate-chrome.so` (BoringSSL)
  with impersonation applied

#### Scenario: Firefox-family profile served by system OpenSSL
- **WHEN** a plugin calls `http.fetch(url, {profile = "ff153"})`
- **THEN** the request is served by system `libcurl.so.4` (OpenSSL) with impersonation
  skipped; the host does NOT silently degrade to the chrome engine

#### Scenario: Impersonate TLS failure falls back to system libcurl
- **WHEN** a chrome-family request fails `CURLE_SSL_CONNECT_ERROR` on two fresh-handle
  attempts against the impersonate lib
- **THEN** the host retries once on system `libcurl.so.4` (OpenSSL) before returning an error

## ADDED Requirements

### Requirement: Honest curl-family target list
`is_known_target(target)` SHALL return true for `chrome*`, `edge*`, `safari*` (impersonate
family) and `ff*`, `firefox*`, `tor*` (system-OpenSSL family). Unknown targets are rejected
before any lib is selected.
