# Spec Delta

## Purpose

Defines the host-tool functions plugins may call — network fetch with browser TLS fingerprinting, logging, and time — as one stable contract that every future runtime binding implements identically.

## ADDED Requirements

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
