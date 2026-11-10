# Proposal

## Why
Config is only editable today via the CLI (`--flag`), env vars, or hand-editing the ini. There is no UI surface; the About page lists knobs read-only at best. The user wants a Settings page in the web UI to view and edit the daemon's configuration without touching the file by hand.

## Current state (2026-11-10)
- Single source of config truth: `src/config/registry.hpp` — 13 knobs across `[server]`, `[plugin]`, `[cache]`, `[paths]`, `[log]` with section/key/flag/env/default/doc/type. `config::dump()` renders the ini template (doc comment + default per knob); `config::parse_updates()` validates a JSON body of `section.key: string|number` and rejects unknown keys / bad numerics before writing; `config::save()` merge-saves into the ini preserving comments/ordering; missing file seeded from `dump()`.
- `POST /config` route (`src/http/http_api.cpp`) consumes exactly that contract: `{"section.key": string|number}` → validation-first merge-save → sets live `plugin.fetch_profile` via `plugins.set_default_profile` without restart. Response: `{ok:true, written:n}` or `{ok:false, error}` with 400/500.
- Registry is exposed nowhere over HTTP as read: GET has no config endpoint; `/config` is POST-only.
- Chrome (top nav in `templates/top.mustache`): Library | Search | History | Plugins | Logs | About.
- Pages are mustache + Alpine.js; htmx for partial swaps. Registration seam: `register_pages` in `src/http/http_pages.cpp`.

## What Changes
- New page `/settings` (mustache + Alpine row per knob) rendering from a new read endpoint.
- New `GET /config` — returns the registry projected through the resolved layers (default < ini < env < explicit flag) plus each knob's registry metadata (section, key, doc, type, default, env, flag), so the UI can render labels/hints and pre-filled values without scraping the ini.
- Page submits to the existing `POST /config` (no new write path; the same validation-first merge-save, same live `fetch_profile` reload).
- Knobs with side effects that need a restart (port, db path, data_dir, log_file, cache_max_mb, image_format/cover_max_px apply to new writes only, log level applies at next boot) are surfaced with a "restart to apply" hint; `plugin.fetch_profile` stays live-editable.
- Top nav gains `Settings`.
- Read path must not require a restart; it re-resolves on every GET (ini may have changed on disk).

## Capabilities
### New Capabilities
- `settings-page`: the Settings page — read (GET /config projection), render, edit (POST /config), restart hints, nav entry

### Modified Capabilities
<!-- none: POST /config contract unchanged -->

## Impact
- `src/http/http_pages.cpp` (+ route `/settings`), `src/http/http_api.cpp` (+ `GET /config`), `src/http/http_routes.hpp`, `templates/top.mustache` (nav), `templates/settings.mustache` (new), `openspec/specs/settings-page/spec.md` (new main spec on archive).
- No config core change unless the GET projection needs a helper (keep it in `src/config/` if so — no config logic outside that dir).
- No plugin ABI change; no store schema change.
- Rollback = revert; ini untouched semantics unchanged.
