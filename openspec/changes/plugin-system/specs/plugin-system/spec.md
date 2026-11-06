# Spec Delta

## Purpose

Defines how plugins are discovered, packaged, versioned, and safely executed, so that manga sources and metadata enrichers can be added without touching host code.

## ADDED Requirements

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
