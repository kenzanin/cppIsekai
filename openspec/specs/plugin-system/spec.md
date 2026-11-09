# plugin-system Specification

## Purpose

Defines how plugins are discovered, packaged, versioned, and safely executed, so that manga sources and metadata enrichers can be added without touching host code.

## Requirements

### Requirement: Plugin directory layout
The host SHALL discover plugins by scanning the directory configured as `plugins` in `data/cppisekai.ini`, one folder per plugin, where each folder contains a `main.json` manifest, an entry file named by the manifest, and an optional logo image (`logo.png`, `logo.svg`, `logo.jpg`, or `logo.avif`).

#### Scenario: Valid plugin is discovered
- **WHEN** the directory contains a folder with a parseable `main.json` manifest and its declared entry file
- **THEN** the plugin appears in the plugin list with its manifest-declared name and id (folder name)

#### Scenario: Broken plugin does not break the host
- **WHEN** a plugin folder has an unparseable `main.json`, a missing entry file, or a manifest missing required fields
- **THEN** the host skips that plugin, logs a warning, and continues loading the remaining plugins

#### Scenario: Duplicate ids
- **WHEN** two plugin folders declare the same id
- **THEN** the host keeps the first in scan order, logs a warning about the skipped duplicate

### Requirement: Manifest contents
The `main.json` manifest SHALL declare `name`, `id` (may be implied by folder name), `version`, `runtime` (`lua`, `wasm`, `js`, or `micropython`), `capabilities` (subset of `search`, `detail`, `chapters`, `enrichment`), and `api` (integer contract version).

#### Scenario: Manifest declares runtime and capabilities
- **WHEN** a manifest declares `runtime: "lua"`, `capabilities: ["search","detail","chapters"]`, and `api: 1`
- **THEN** the host registers the plugin for exactly those capabilities under the Lua runtime

### Requirement: API version contract
The host SHALL call plugins with a JSON envelope `{"api": <host api>, "method": "<method>", "params": {...}}` and SHALL accept `{"ok": true, "data": ...}` or `{"ok": false, "error": "<message>"}` as the only successful response shapes. A plugin whose manifest `api` is greater than the host's supported version SHALL NOT be executed; a plugin with an older `api` MAY be executed best-effort.

#### Scenario: Plugin from the future
- **WHEN** a manifest declares `api: 2` and the host supports api 1
- **THEN** the plugin is skipped with a clear log message naming both versions

#### Scenario: Method call round-trip
- **WHEN** the host sends the envelope with `method: "search"` and valid params
- **THEN** the plugin's returned `{"ok": true, "data": ...}` is parsed by the host, or `{"ok": false, "error": ...}` is surfaced as a logged failure

#### Scenario: Malformed response
- **WHEN** a plugin returns output that is neither valid JSON nor matches the ok/error shape
- **THEN** the host treats the call as failed and logs the raw output

### Requirement: Lua sandbox restrictions
The Lua runtime SHALL expose only the whitelisted stdlibs (`base`, `string`, `table`, `math`, `json`) plus a read-only subset of `io` (`io.lines`, `io.read`); `os` and `debug` SHALL NOT be available, and file-writing through `io` SHALL NOT be possible.

#### Scenario: Plugin tries to escape the sandbox
- **WHEN** a plugin calls `os.execute`, `debug.sethook`, or attempts `io.open(path, "w")`
- **THEN** the call fails inside the plugin and the host surfaces it as a plugin error, without harming the host process

#### Scenario: Allowed work succeeds
- **WHEN** a plugin uses only `string`, `table`, `math`, `json`, `io.lines`, and host tools
- **THEN** the plugin executes normally

### Requirement: Runaway plugin termination
The host SHALL impose a per-call instruction-count timeout on Lua plugin execution, so a plugin that loops forever returns a timeout error to the host instead of hanging it.

#### Scenario: Infinite loop plugin
- **WHEN** a plugin enters an endless loop during a call
- **THEN** the host receives a timeout error for that call and continues serving requests

### Requirement: Reload endpoint
The host SHALL expose `POST /plugins/reload` that rescans the plugin directory and re-registers plugins without restarting the process, preserving the same discovery rules as startup.

#### Scenario: Reload after editing a plugin
- **WHEN** a plugin file is edited and `POST /plugins/reload` is invoked
- **THEN** subsequent plugin calls use the edited code without a host restart

### Requirement: Non-Lua runtime execution
The host SHALL execute plugins whose manifest declares `runtime: "js"` or `runtime: "wasm"`
under the same JSON envelope contract as Lua: the plugin receives `{api, method, params}`
and the host accepts only `{"ok": true, "data": ...}` or `{"ok": false, "error": "<msg>"}`
as the successful response shapes. A runtime that fails to load its entry SHALL leave the
plugin unregistered, with the failure recorded in the host log; already-registered plugins
are unaffected.

#### Scenario: JS plugin answers a call
- **WHEN** a registered `js` plugin whose entry defines `handle` is called via
  `GET /plugin/<id>/<method>`
- **THEN** the host returns the plugin's `{"ok":true,...}` result shaped like any Lua plugin
  response

#### Scenario: WASM plugin answers a call
- **WHEN** a registered `wasm` plugin exporting the documented `handle` entry is called
- **THEN** the host passes the envelope JSON to the module and returns the module's response
  unchanged in shape

#### Scenario: Broken runtime entry
- **WHEN** a `js` or `wasm` plugin's entry cannot be loaded (parse/compile/instantiate
  failure)
- **THEN** the plugin does not appear in the plugin list, the failure is logged, and other
  plugins keep serving

### Requirement: Host-tool parity across runtimes
All host-provided functions live under a single `host.*` namespace in every plugin
runtime, with identical names, argument order, and return shapes. The host-tool set is:
`host.now`, `host.log.info`, `host.log.error`, `host.http.fetch`, `host.storage.get`,
`host.storage.set`, `host.fs.read`, `host.re2.match`, `host.re2.find`,
`host.html.select`, `host.html.attr`. A behavior difference between runtimes in these
tools is a defect. WASM exposes the same set as flat `env.*` imports
(`env.<group>_<fn>`) because wasm imports cannot nest.

#### Scenario: Same tool semantics in js and lua
- **WHEN** a `js` plugin and a `lua` plugin each call `host.http.fetch` against an
  unreachable host with the same options
- **THEN** both observe the same error shape and neither crashes the host

#### Scenario: Parity across lua, js, and wasm
- **WHEN** a fixture plugin in each runtime (lua, js, wasm) inspects its host-tool
  surface and calls `host.now`, `host.http.fetch`, and `host.storage.set`/`get`
- **THEN** every runtime resolves the full 11-tool set
- **AND** the returned JSON shapes are identical across the three runtimes

#### Scenario: Lua gains fs.read parity
- **WHEN** a lua fixture calls `host.fs.read("config.json")` from its plugin directory
- **THEN** the file body is returned under the same path confinement rules js/wasm use
- **AND** paths escaping the plugin directory are rejected identically to js/wasm

### Requirement: Regex host helper (RE2)
`host.re2.match(pattern, text)` SHALL return an array of capture groups (element 0 =
whole match, then submatches; empty array when the pattern does not match).
`host.re2.find(pattern, text)` SHALL return the first match in the same array form, or
an empty array when there is no match. Patterns SHALL be evaluated with linear-time RE2.

#### Scenario: Match and find on fixture text
- **WHEN** a plugin calls `host.re2.match("ch(\\d+)", "ch1050")` and
  `host.re2.find("(\\w+)@(\\w+)", "user@host other")`
- **THEN** match returns `["ch1050", "1050"]`
- **AND** find returns `["user@host", "user", "host"]`

#### Scenario: No match is not an error
- **WHEN** the pattern does not appear in the text
- **THEN** the result is an empty array and no error is raised

#### Scenario: Invalid pattern is loud
- **WHEN** the pattern fails RE2 compilation
- **THEN** the calling script receives a runtime error (lua error / JS exception /
  wasm negative rc) naming the compile failure

### Requirement: DOM host helpers (lexbor)
`host.html.select(html, css)` SHALL return the trimmed text contents of all elements
matching the CSS selector. `host.html.attr(html, css, attr)` SHALL return the values of
the named attribute for all matching elements that have it (elements without the
attribute are skipped). No-match SHALL yield an empty array; malformed HTML SHALL be
parsed leniently.

#### Scenario: Select text from fixture HTML
- **WHEN** a plugin calls `host.html.select(fixture_html, "div.title > a")`
- **THEN** the array holds the trimmed text of each matched anchor in document order

#### Scenario: Attribute extraction skips missing
- **WHEN** matching elements include ones without the requested attribute
- **THEN** only elements that have the attribute contribute a value, in document order

#### Scenario: Empty results for no match and bad HTML
- **WHEN** the selector matches nothing or the HTML is truncated/malformed
- **THEN** the helpers return an empty array and do not throw

### Requirement: Confined read-only plugin file access
The host SHALL provide a read-only file-read tool (`fs.read`) to every runtime. Given a
relative path, it SHALL return the contents of a regular file resolved inside that
plugin's own folder; paths escaping the folder (absolute paths, `..` traversal) and
missing files SHALL yield a not-found result, never an error response or a read outside
the folder.

#### Scenario: Plugin reads its own config
- **WHEN** a plugin calls `fs.read("config.json")` and the file exists in its folder
- **THEN** the tool returns the file contents

#### Scenario: Path escape and missing file
- **WHEN** a plugin calls `fs.read("../other/secret")` or `fs.read("nope.json")`
- **THEN** the tool reports not-found without reading outside the plugin folder in either
  case

### Requirement: Runtime resource limits
Long-running plugin code SHALL be bounded in every runtime: the host SHALL apply a
computation budget derived from the plugin timeout configuration to `wasm` and `js`
executions, mirroring the existing Lua instruction budget, and a budget overrun SHALL
produce a `{"ok":false,"error":...}` response, not a hang.

#### Scenario: Runaway plugin loop
- **WHEN** a `js` (or `wasm`) plugin entry loops without returning
- **THEN** the host returns an ok:false budget-exceeded response within the configured
  bound

### Requirement: Per-call impersonation profile
The host fetch gate SHALL accept a profile identifier on every request; when present it
SHALL override the configured default for that call only, and an unknown profile SHALL
fall back to the configured default with a warning, never a failed request. Changing the
configured default SHALL take effect for subsequent calls without host restart or plugin
reload.

#### Scenario: Per-call override
- **WHEN** a plugin passes `profile: "edge101"` in request options while the configured
  default is `chrome116`
- **THEN** that single request impersonates edge101 and later requests without the option
  use chrome116

#### Scenario: Live default change
- **WHEN** `POST /config` sets `plugin.fetch_profile` to another known target
- **THEN** the next plugin request without an explicit profile uses the new value without
  host restart

#### Scenario: Firefox-family target
- **WHEN** a plugin passes a `firefox*` profile while the default stays a chrome-family
  target
- **THEN** that request runs on the firefox engine's target via the family vtable and
  later chrome-family defaults are unaffected

### Requirement: Persistent per-plugin cookie jar
The host fetch gate SHALL support a per-plugin cookie jar stored at
`data/plugin-state/<id>.cookies.txt`, enabled per request via options; cookies SHALL
persist across host restarts and SHALL NOT be shared between plugins. When the option is
absent the request SHALL behave exactly as before the feature (no cookie engine).

#### Scenario: Session survives restart
- **WHEN** a plugin with an enabled jar receives session cookies and the host restarts
- **THEN** the next request from that plugin reuses the stored cookies

#### Scenario: Isolation between plugins
- **WHEN** two plugins with separate jars fetch from the same site
- **THEN** neither observes the other's cookies
