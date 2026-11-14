# Design: plugin-config-and-storage

## Context

MangaDex API now requires OAuth2 for authenticated endpoints. The plugin ABI envelope and
manifest format are frozen (spec `plugin-system`); this change only extends host tools and
the mangadex plugin.

## Goals / Non-Goals

- Goals: plugins can POST form data with custom headers; plugins can read user-provided
  config; plugins can persist small state across calls and restarts; mangadex works with
  or without credentials.
- Non-Goals: public client flow (authorization_code — MangaDex hasn't shipped it);
  multi-account support; secrets encryption at rest; generic HTTP verbs beyond what curl
  custom requests already provide.

## Decisions

**1. Generic fetch options, not a separate `http.post`.**
`http.fetch(url, {method?, body?, headers?})` — one binding to document and bind per
runtime. `method` defaults to `"GET"`; `headers` is a string-keyed table merged after the
built-in Accept/User-Agent so a plugin can override either. Content-Type for a `body` is
the caller's responsibility via `headers` (the auth POST sets
`application/x-www-form-urlencoded` itself).

**2. Credentials in `config.json`, read by the host, keyed access from Lua.**
`config.get(key)` returns the value or nil; the host reads the file once per plugin load
and caches the parsed table. Missing file = plugin runs unauthenticated — same code path
as today. `config.json` is gitignored; `config.example.json` documents the shape:
`{"client_id": "...", "client_secret": "...", "username": "...", "password": "..."}`.

**3. Token state in a DB key-value table, not in Lua globals or files.**
Table `plugin_state(plugin_id TEXT, key TEXT, value TEXT, PRIMARY KEY(plugin_id, key))`,
accessed via `Store`. `storage.get/set` are plain string in/out (JSON-encoded by the plugin
if it needs structure). Access token expiry is plugin logic: store `expires_at` (unix
seconds, `now()` + lifetime), refresh when past it or on a 401 from the API, single
retry-then-error — no background refresh timer.

**4. Auth header scoping is plugin discipline, enforced by review not by host code.**
MangaDex forbids auth headers on image domains; the host does not police per-domain
headers (that would need a domain policy engine — YAGNI until a second plugin needs it).
The mangadex plugin only attaches `Authorization` to `api.mangadex.org` URLs.

## Risks / Trade-offs

- [config.json in plaintext] → same trust level as the host `.ini`; documented, gitignored
- [storage values as strings] → structure via json.encode; sufficient for tokens
- [401-after-refresh race on concurrent calls] → per-state mutex already serializes calls
  per plugin; acceptable

## Migration Plan

1. Add `plugin_state` table + Store methods.
2. Extend `http.fetch`; add `config.get`, `storage.get/set` bindings.
3. Add `config.example.json`, `.gitignore` entry, mangadex auth logic.
4. Manual verify: unauth path unchanged; auth path with real personal client creds.
