# js-runtime Specification

## Purpose
TBD - created by archiving change plugin-port-multi-runtime. Update Purpose after archive.

## Requirements

### Requirement: JS (quickjs-ng) plugin runtime
The host SHALL load JS plugins via quickjs-ng under `src/plugin/js/`, exposing host tools (`http.fetch`, `storage.get/set`, `log.*`, `crypto.*`) as a `host` global object to plugin code.

#### Scenario: JS plugin entrypoint
- **WHEN** a plugin folder `plugins_dev/js/<site>/` contains `main.json` and `main.js`
- **THEN** the quickjs-ng loader instantiates the plugin and routes JSON-text-in/out ABI calls to `main.js` exports

#### Scenario: Host tools available to JS
- **WHEN** a JS plugin calls `host.http.fetch(url, opts)`
- **THEN** the host performs the HTTP request through the same curl-family gate as Lua plugins and returns `{status, body, headers}`

### Requirement: Plugin ABI contract
The JSON-text-in/out ABI (search_manga, get_manga_detail, get_chapter_list, get_page_list, optional get_genres, PLUGIN manifest) SHALL be runtime-agnostic; JS plugins SHALL implement the same exports as Lua plugins.

#### Scenario: Same exports in JS and Lua
- **WHEN** the host dispatches `chapters` to a JS plugin and to a Lua plugin
- **THEN** both are invoked with the same `{api, method, params}` JSON and both answer with the same `{ok, data}` envelope
