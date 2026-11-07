# plugin-host-tools delta

## MODIFIED Requirements

### Requirement: HTTP fetch with browser fingerprint
Plugins SHALL call `http.fetch(url, options)` to perform an HTTP request through the host's
curl-impersonate client. `options` MAY contain `timeout_ms`, `profile`, `method` (default
`"GET"`), `body` (request body string), and `headers` (string-keyed table of request
headers, applied after built-ins so plugins can override Accept/User-Agent). When
`timeout_ms`/`profile` are absent, the host SHALL use the global defaults from
`data/cppisekai.ini`. The result SHALL be a table with `status` (HTTP code), `body`
(response body as text), and `headers`.

#### Scenario: Fetch with defaults
- **WHEN** a plugin calls `http.fetch("https://api.example.com/manga")` with no options
- **THEN** the request uses the `.ini`-configured timeout and TLS profile and returns `status`, `body`, and `headers`

#### Scenario: Per-call override
- **WHEN** a plugin calls `http.fetch(url, {timeout_ms = 5000, profile = "chrome116"})`
- **THEN** that single request uses 5000 ms and the chrome116 profile, leaving the global defaults unchanged

#### Scenario: POST with form body and custom header
- **WHEN** a plugin calls `http.fetch(url, {method = "POST", body = "grant_type=password&...", headers = {["Content-Type"] = "application/x-www-form-urlencoded", Authorization = "Bearer x"}})`
- **THEN** the request is sent as POST with exactly that body and those headers, and the response arrives as a normal fetch result

#### Scenario: Network failure
- **WHEN** the target is unreachable or times out
- **THEN** `http.fetch` returns a failure the plugin can detect, and the host logs the URL and the error

## ADDED Requirements

### Requirement: Plugin config access
Plugins SHALL call `config.get(key)` to read a value from a `config.json` file in the
plugin's own folder, written by the user and never read by the plugin loader. A missing
file or missing key SHALL return nil. The host SHALL parse the file; the Lua sandbox SHALL
NOT gain file-write or direct file-read access.

#### Scenario: Credentials present
- **WHEN** `data/plugins/mangadex/config.json` contains `{"client_id": "c", "username": "u"}` and the plugin calls `config.get("client_id")`
- **THEN** it receives `"c"`; `config.get("nope")` returns nil

#### Scenario: No config file
- **WHEN** the plugin folder has no `config.json`
- **THEN** every `config.get` returns nil and the plugin loads normally

### Requirement: Plugin state storage
Plugins SHALL call `storage.get(key)` and `storage.set(key, value)` to persist string
values scoped to their own plugin id, backed by the host database. Values SHALL survive
plugin reload and host restart; a plugin SHALL NOT be able to read another plugin's state.

#### Scenario: State survives restart
- **WHEN** a plugin calls `storage.set("refresh_token", "r")`, the host restarts, and the plugin calls `storage.get("refresh_token")`
- **THEN** it receives `"r"`
