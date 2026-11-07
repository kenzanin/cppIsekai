# Design: plugins-page

## Context

Host serves a mustache page per route; data reaches pages via JSON endpoints the page JS
fetches. The plugin table exists; the Host already tracks runtimes in memory and records
nothing about call outcomes today.

## Goals / Non-Goals

- Goals: see health at a glance, disable noisy/broken plugins without DB surgery, jump to
  a plugin's titles, reload after editing plugin files.
- Non-Goals: config/kredensial editor di UI, uninstall, per-plugin metrics, call history
  (in-memory last-error only), per-plugin granular reload (global rescan is <100ms at
  current scale; row button triggers it).

## Decisions

**1. `enabled` lives in the DB, checked in the request path.**
`plugin.enabled INTEGER DEFAULT 1`. The http layer flips it via `Store` on the db pool; the
ABI route checks it (same pool) before dispatching to the plugin pool and answers
`{"ok":false,"error":"plugin disabled"}` when off. The plugin runtime stays loaded — disable
is a routing flag, not a teardown; re-enable is instant.
User ruling (2026-11-07): search is plugin-gated — the user picks a plugin to search with.
Consequences: the search page's plugin picker lists ENABLED plugins only; a disabled plugin
neither appears there nor answers calls. Disabled rows stay on the plugins page (greyed,
toggle to re-enable) and their titles leave the library default view.

**2. Health = last call outcome, in `plugin::Host` memory.**
After every `call()`, Host records `{ok, error, at}` per plugin id (guarded by the existing
mutexes). `GET /api/plugins` joins store rows + this map: `disabled` flag wins, else
`error` if the last call failed, else `active`. No persistence — restart resets to active;
the next call repaints it. Good enough for a live dashboard.

**3. Page data in one endpoint; page renders from it.**
`GET /api/plugins` returns the row array (id, name, version, runtime, capabilities, enabled,
health, titles, has_logo). The mustache template is a shell; a small vanilla-JS fetch
renders rows and wires the toggle/reload/search. Search is client-side substring on
name/capability.

**4. Count click = plain link, not JS.**
`<a href="/library?plugin=<id>">` — the library page already filters by plugin; no new
mechanism.

**5. Logo fallback is server-trivial.**
`has_logo` bool from the existing logo lookup; template renders the initial span when
false. No placeholder image asset.

## Risks / Trade-offs

- [Health resets on restart] → acceptable; error is transient state, not audit log
- [Disabled check adds a db hit per call] → single indexed column on the db pool; calls
  are network-bound
- [Global reload per row] → N is tiny; revisit when plugins >10

## Migration Plan

1. Store: column + setter + tests.
2. Host: health record + enabled arg on call path... (kept: http checks enabled, Host
   records outcome) + expose health snapshot.
3. HTTP: `/api/plugins`, `POST /plugin/<id>/enabled`, page template + js.
4. Manual pass: toggle→library filter→call-shape; reload button; empty state with no rows.
