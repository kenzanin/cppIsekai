# Proposal: plugins-page

## Why

The plugins page is a blank stub. With a real plugin ecosystem forming (mangadex live, more
runtimes planned), there is no place to see what is installed, whether it works, or to turn
a broken plugin off without editing the DB by hand. User-approved scope: 3-state health
status, full control row.

## What Changes

- New `plugins` page listing every row in the `plugin` table: logo (with initial fallback),
  name, version, runtime badge, capability chips, 3-state status
  (`active` / `error` / `disabled`) with last error message, title count linking to
  `/library?plugin=<id>`, enable/disable toggle, reload button.
- `plugin.enabled` column (default on) + `Store::set_plugin_enabled`; disabled plugins are
  greyed out, excluded from library filters, and ABI calls return
  `{"ok":false,"error":"plugin disabled"}`.
- `GET /api/plugins` page data (rows + health + counts); `POST /plugin/<id>/enabled`;
  per-row reload reuses the existing global rescan.
- Health source: host records last call outcome per plugin in memory (ok/error + message);
  resets on restart — history is a later problem.

## Capabilities

### New Capabilities
- `plugins-page` — the page, its data contract, status semantics, toggle/reload behavior,
  disabled-plugin behavior.

### Modified Capabilities
- (none — disabled-call shape documented under `plugins-page`)

## Impact

- `src/store`: `enabled` column + setter (migration via sync_schema).
- `src/plugin`: per-call health record + `enabled` check in call path, last-error exposure.
- `src/http`: `/plugins` page data endpoint + toggle; templates/static for the page.
- Tests: store default/toggle, call-disabled shape, page data shape.
