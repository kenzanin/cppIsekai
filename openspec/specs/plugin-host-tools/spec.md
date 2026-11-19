# plugin-host-tools Specification

## Purpose

Defines the host-tool functions plugins may call — network fetch with browser TLS fingerprinting, logging, and time — as one stable contract that every future runtime binding implements identically.

## Requirements

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

### Requirement: Crypto host tool `crypto.vrf_sign`
Plugins SHALL call `crypto.vrf_sign(api_path, params, stages)` to sign an upstream request, mangafire-style. The host SHALL trim a leading `/api` from the sign string, serialize params sorted by key as raw `k=v` pairs, and apply stages in order as `out[i] = tbl[(d ^ key[i % len] ^ prev) & 0xFF]`, `prev` seeded to `iv`. The result SHALL be RawURL base64url without padding. Stages (`iv`, standard-base64 `key`/`tbl`) are plugin-declared. Invalid stage data SHALL fail without a crash.

#### Scenario: Deterministic sign of a known request
- **WHEN** the plugin calls `crypto.vrf_sign("/titles", {limit="50"}, VRF_STAGES)` with a fixed table set
- **THEN** the result is exactly the base64url signature captured in the change's test vectors, and repeated calls return the same string

#### Scenario: Empty params edge
- **WHEN** the plugin passes an empty params table
- **THEN** the signature input omits the `?` separator entirely and the result is the signature of the bare path

#### Scenario: Invalid stage rejected cleanly
- **WHEN** a stage's `key` or `tbl` is not valid standard base64
- **THEN** the call fails with a `vrf_sign: stage <n>: ...` reason, the plugin receives a detectable failure, and the host logs the reason tagged with the plugin id

#### Scenario: Tables stay plugin-side
- **WHEN** a plugin rebuilds its table set after a site rotation
- **THEN** only the plugin file changes; the host binary is not recompiled

### Requirement: Host-tool parity across runtimes
Every host tool on the Lua surface SHALL be implementable by the JS and WASM bindings with the same request/response semantics, or the gap SHALL be explicitly documented as out of scope in the runtime's contract. The parity set is:

| Tool | Lua | JS | WASM |
|---|---|---|---|
| `now` | yes | yes | yes |
| `log.info` / `log.error` | yes | yes | yes (`log_info`; error maps to the same import with a level byte) |
| `http.fetch` | yes | yes | yes |
| `storage.get` / `storage.set` | yes | yes | yes |
| `fs.read` | yes | yes | yes |
| `config.get` | yes | yes (this change) | out of scope |
| `url_encode` / `url_decode` | yes | yes (this change) | yes (this change) |
| `crypto.vrf_sign` | yes | yes (this change) | out of scope |
| `re2.match` / `re2.find` | yes | yes | yes |
| `html.select` / `html.attr` | yes | yes | yes |

`html.select`/`html.attr` double as the list selectors: they return arrays of all matched elements (trimmed text for `select`, attribute string for `attr`) in all three runtimes — there is no separate `find_list_*` tool and none is added. Ports migrating goIsekai plugins SHALL map `host.html.find_list_text(html, css)` → `host.html.select(html, css)` and `host.html.find_list_attr(html, css[, attr])` → `host.html.attr(html, css, attr)` (default attr `text` where applicable).

#### Scenario: url_encode round-trips in JS and WASM
- **WHEN** a JS or WASM plugin encodes a string with reserved characters (`a b&c=d%?`)
- **THEN** the encoded form percent-encodes exactly the characters that `url_decode` accepts to return the original string

#### Scenario: config.get in JS matches Lua behavior
- **WHEN** a JS plugin calls `config.get("client_id")` with a `config.json` containing `client_id` and then for an absent key
- **THEN** it receives the stored value for the present key and nil for the absent key

#### Scenario: vrf_sign in JS matches the Lua vectors
- **WHEN** a JS plugin calls `crypto.vrf_sign("/titles", {limit="50"}, VRF_STAGES)` with the same fixed table set used in `tests/test_vrf.cpp`
- **THEN** the result equals the base64url signature the Lua binding produces for the same arguments

#### Scenario: http.fetch opts behave the same in all three runtimes
- **WHEN** a JS or WASM plugin passes `{timeout_ms, profile, method, body, headers}`
- **THEN** the request is issued with those values exactly as the Lua path would, and the reply shape is `{status, body, headers}`

### Requirement: First-capture regex helper
Plugins SHALL call `re2.capture(pattern, text)` to obtain one captured value without writing a
table shim. It SHALL return capture group 1 when the pattern declares groups, the whole match
when it declares none, and nil when the pattern does not match. An empty captured group SHALL
return the empty string, not nil. `re2.match`/`find`/`find_all` keep their existing 1-based
table semantics (index 1 = full match, group *n* = index *n+1*).

#### Scenario: Group present
- **WHEN** a plugin calls `re2.capture('<a href="([^"]+)"', '<a href="/x">')`
- **THEN** the result is `/x`

#### Scenario: No group declared
- **WHEN** a plugin calls `re2.capture('class="x"', 'foo class="x" bar')`
- **THEN** the result is the matched text `class="x"`

#### Scenario: No match
- **WHEN** the pattern does not occur in the text
- **THEN** the result is nil and no error is raised

### Requirement: Bounded JSON blob extraction
Plugins SHALL call `text.json_blob(text, marker)` to extract a JSON document embedded in HTML.
The host SHALL return the first balanced `{...}` or `[...]` beginning at or after `marker`,
scanning string-escape aware (`"` toggles string state, `\` escapes the next byte) and tracking
nesting depth. It SHALL return nil when the marker is absent, no balanced body follows, or the
input is truncated. The returned value SHALL be raw JSON text; decoding remains `json.decode`.

#### Scenario: State blob after marker
- **WHEN** the page contains `window.__DATA__ = {"a":{"b":"}}"} ;` and the plugin calls `text.json_blob(html, "window.__DATA__ = ")`
- **THEN** the result is exactly `{"a":{"b":"}}"}` — braces inside strings do not close it

#### Scenario: Marker absent
- **WHEN** the marker does not occur in the document
- **THEN** the result is nil

#### Scenario: Truncated payload
- **WHEN** the document ends before the JSON body is balanced
- **THEN** the result is nil, not a partial blob

### Requirement: Date-to-epoch helper
Plugins SHALL call `text.date_epoch(raw)` to convert a site date string to unix seconds. The host
SHALL parse with the same parser used by `text.date_to_iso` (layouts: `YYYY-MM-DD`,
`YYYY-MM-DDTHH:MM:SS` with optional `Z`, `±HH:MM` offset or fractional seconds, `Mon DD, YYYY`,
`DD Mon YYYY`, and `HH:MM`-suffixed variants) and SHALL interpret zone-less input as UTC. An
unparsable value SHALL return nil and SHALL NOT fall back to the current time.

#### Scenario: ISO date
- **WHEN** a plugin calls `text.date_epoch("2026-03-04")`
- **THEN** the result is the unix timestamp of 2026-03-04T00:00:00Z

#### Scenario: Human date
- **WHEN** a plugin calls `text.date_epoch("Mar 4, 2026")`
- **THEN** the result equals the ISO form above

#### Scenario: Garbage input
- **WHEN** the value cannot be parsed (empty, `"unknown"`, site placeholder text)
- **THEN** the result is nil and the plugin decides the fallback

### Requirement: URL reference resolution
Plugins SHALL call `url_abs(base, ref)` to turn a reference found in markup into an absolute URL.
The host SHALL return `ref` unchanged when it starts with `http://` or `https://`; prepend the
base scheme when `ref` starts with `//`; prepend the base origin when `ref` starts with `/`; and
otherwise resolve it relative to the base path truncated at its last `/`. Dot-segment
normalization and query/fragment merging are out of scope — callers needing them handle it
plugin-side.

#### Scenario: Protocol-relative
- **WHEN** a plugin calls `url_abs("https://site.tld/manga/x", "//cdn.site.tld/a.jpg")`
- **THEN** the result is `https://cdn.site.tld/a.jpg`

#### Scenario: Root-relative
- **WHEN** a plugin calls `url_abs("https://site.tld/manga/x", "/img/a.jpg")`
- **THEN** the result is `https://site.tld/img/a.jpg`

#### Scenario: Already absolute
- **WHEN** `ref` starts with `https://`
- **THEN** it is returned byte-identical

### Requirement: Two-value body fetch
`http.fetch_body(url, options)` SHALL return the body and an error string. On success the error
SHALL be nil. On a non-200 response the body SHALL be nil and the error SHALL name the upstream
status. On transport failure the body SHALL be nil and the error SHALL carry the transport
error. Body and error SHALL never both be nil.

#### Scenario: Upstream error is diagnosable
- **WHEN** a plugin calls `http.fetch_body(url)` and upstream answers 410
- **THEN** the body is nil and the error is `upstream status 410`

#### Scenario: Transport failure
- **WHEN** the host cannot connect to the upstream host
- **THEN** the body is nil and the error is a non-empty transport message, logged with the plugin id

### Requirement: JSON fetch with full options
`http.get_json(url, options)` SHALL accept the same option table as `http.fetch` and SHALL return
the decoded value plus an error. When the table contains none of the known option keys it SHALL
be treated as a headers map.

#### Scenario: JSON fetch with a TLS profile
- **WHEN** a plugin calls `http.get_json(url, {profile = "chrome116", timeout_ms = 8000, headers = {Accept = "application/json"}})`
- **THEN** the request uses that profile, timeout and header, and the decoded value is returned

#### Scenario: Legacy headers map still works
- **WHEN** a plugin calls `http.get_json(url, {Accept = "application/json"})`
- **THEN** the table is used as request headers and no option parsing error occurs

### Requirement: Shared read-only plugin library
The host SHALL resolve `require(name)` in the plugin directory first and then in a shared library
root `<plugins dir>/../plugins_lib`, both read-only, with module-name traversal (absolute paths,
`.`/`..` segments) rejected in both roots and a per-plugin-state module cache. A plugin-local
module SHALL shadow a shared module of the same name. The plugin sandbox SHALL NOT gain write or
arbitrary-path read access.

#### Scenario: Shared module resolves
- **WHEN** `mangabay/main.lua` calls `require("dle")` and only `plugins_lib/dle.lua` exists
- **THEN** the module loads from the shared root

#### Scenario: Local module shadows shared
- **WHEN** both `plugins/<id>/dle.lua` and `plugins_lib/dle.lua` exist
- **THEN** the plugin's own copy is used

#### Scenario: Escaping the roots is refused
- **WHEN** a plugin calls `require("../etc/passwd")`
- **THEN** the load fails with a `require: bad module name` error and no file outside the roots is read

### Requirement: Promoted-helper parity
The promoted helpers SHALL be exposed on each runtime able to carry them without new host state. Gaps SHALL be recorded in this table:

| Tool | Lua | JS | WASM |
|---|---|---|---|
| `re2.capture` | yes | yes | no |
| `text.json_blob` | yes | yes | no |
| `text.date_epoch` | yes | yes | no |
| `url_abs` | yes | yes | no |
| `fetch_body` 2-value | yes | no | no |
| `get_json` opts | yes | yes | no |
| `require` shared root | yes | n/a | n/a |

#### Scenario: JS and Lua agree on capture
- **WHEN** a JS plugin and a Lua plugin call `re2.capture` with the same pattern and text
- **THEN** both receive the same string, or nil in both

### Requirement: Runtime gaps stay inert
A runtime carrier marked absent in the parity table SHALL keep its present surface: WASM guests
SHALL link and run with their existing import set, and JS SHALL keep the single-value
`fetch_body` form until a JS plugin needs the error string.

#### Scenario: WASM guest is unaffected
- **WHEN** an existing WASM plugin is loaded after this change
- **THEN** it links and runs with its present import set, no new required imports
