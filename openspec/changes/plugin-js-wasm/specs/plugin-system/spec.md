# Spec Delta

## ADDED Requirements

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
The host tools `http.fetch`, `storage.get`/`storage.set`, `log.info`/`log.error`, `now`,
and `fs.read` SHALL be available with the same names and semantics in every enabled
runtime; a behavior difference between runtimes in these tools is a defect.

#### Scenario: Same tool semantics in js and lua
- **WHEN** a `js` plugin and a `lua` plugin each call `http.fetch` against an unreachable
  host with the same options
- **THEN** both observe the same error shape and neither crashes the host

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
