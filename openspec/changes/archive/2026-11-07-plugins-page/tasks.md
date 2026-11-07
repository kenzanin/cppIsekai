# Tasks: plugins-page

## 1. Store
- [x] 1.1 `plugin.enabled` column (default 1) + `set_plugin_enabled(id, bool)` + include in `plugins()` result (+`runtime` column for the badge, set at upsert)

## 2. Host / behavior
- [x] 2.1 Health map in `plugin::Host`: record ok/error+message+timestamp per call; snapshot accessor
- [x] 2.2 ABI route: disabled check -> `{"ok":false,"error":"plugin disabled"}` (before plugin pool dispatch)

## 3. HTTP + UI
- [x] 3.1 `GET /api/plugins` — rows + enabled + health + title counts + has_logo
- [x] 3.2 `POST /plugin/<id>/enabled` body `{enabled:bool}` via db pool
- [x] 3.3 `plugins.mustache` + JS: rows, caps chips, 3-state badge + error line, toggle, reload (calls `/plugins/reload` then refresh), client-side search, logo fallback, empty state
- [x] 3.4 Search page picker + library filter: enabled plugins only

## 4. Verification
- [x] 4.1 Store: default enabled=1; toggle persists (test_store)
- [x] 4.2 Disabled call shape + search picker exclusion (curl + page)
- [x] 4.3 Error state shows after forced failing call; active after recovery
- [x] 4.4 Count deep-link lands filtered; reload picks up edited main.lua
