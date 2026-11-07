# Proposal: plugin-config-and-storage

## Why

MangaDex now protects its API with OAuth2 (auth.mangadex.org). Personal clients use the
`password` flow: POST form-urlencoded credentials → `access_token` (15-minute lifetime) +
`refresh_token`, refreshed via the token endpoint and sent as `Authorization: Bearer` to
`api.mangadex.org` only. The current host tools (`http.fetch` GET-only, no header/body
support, no persistence) cannot express this, and the plugin has no place to keep
credentials or token state.

Decision recorded in OKF (`decisions/plugin-data-segregation`): the host `.ini` is
host-only config; plugin credentials live in a gitignored `config.json` inside the plugin
folder (never read by the loader, never in the manifest); dynamic state is stored via a
host-provided key-value host tool scoped per plugin id.

## What Changes

- Extend `http.fetch` options with `method`, `body`, and `headers` (generic map; covers
  the auth token POST and `Authorization` headers).
- Add `config.get(key)` host tool: reads the plugin folder's `config.json` (host reads the
  file; the sandbox still has no io). Missing file / missing key → nil.
- Add `storage.get(key)` / `storage.set(key, value)` host tools: per-plugin key-value
  state backed by the DB, survives reload and restart.
- Extend the mangadex plugin: when `config.json` carries personal-client credentials, it
  authenticates, refreshes tokens when expired/rejected, and sends `Authorization` on API
  calls (never to image domains); without `config.json` it runs unauthenticated as today.

## Capabilities

### New Capabilities
- (none)

### Modified Capabilities
- `plugin-host-tools`: generic fetch options (method/body/headers), `config.get`,
  `storage.get`/`storage.set`.
- `mangadex-plugin`: optional-auth behavior — login/refresh flow, header scoping, and
  graceful fallback to unauthenticated calls.

## Impact

- Code: `src/plugin/plugin.cpp` (bindings, state table access), `data/plugins/mangadex/`
  (`config.example.json`, `main.lua` auth logic), `.gitignore` (`config.json`).
- DB: one new key-value table for plugin state (schema migration).
- No change to the manifest format, the ABI envelope, or any host `.ini` key.
