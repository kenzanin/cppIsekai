# Proposal

## Why

The library should quietly learn which followed titles have fresh chapters, without hammering scanlation sources — the user explicitly wants the daemon to be "sadar diri" (self-aware) towards the sites it scrapes. Today updates only happen when the user remembers to refresh a title manually; a gentle, bounded auto-update closes that gap while keeping request volume small and predictable.

## What Changes

- New `[update]` section in the ini / config registry with an auto-update interval (integer, minimum 1 day, default 1), a per-plugin batch cap (`per_plugin_limit`, integer, minimum 1, default 10), and a pacing delay between requests (`request_delay_ms`, integer, minimum 0, default 1000), all editable from `/settings` like the other knobs.
- A background scheduler that, on the configured interval (plus a randomized jitter of a few minutes), picks up to `per_plugin_limit` eligible library titles **per plugin** per run and refreshes only their chapter lists (not detail metadata). The cap is per-plugin (not global) so every site is worst-case bounded independently.
- Candidate filter: only saved manga whose fully-read chapter count is `<= 1` ("not started / barely read") participate in auto-update.
- A new `update_attempts` table tracks the last update attempt per `(plugin, slug)` — timestamp, outcome, and error — so batches rotate across the library instead of re-hitting the same titles, and so the UI and logs can explain why a title was or was not checked.
- `POST /api/refresh/<plugin>/<slug>` (existing refresh path) records an attempt too, so a manual refresh exempts the title from the auto-update batch for the interval.
- Manual update button on the manga detail page (already exists as the refresh button) keeps working and now also stamps the attempt tracker; auto-update never starves manual refresh.
- Behavior-limiting rules: at most `per_plugin_limit` titles per plugin per run, one attempt per title per interval, exponential backoff / skip on 429 or repeated failure, and a per-run request pacing delay configurable via `[update] request_delay_ms` (default 1000 ms between titles).
- No non-goals in this change: no notification system, no push, no per-title opt-out UI (plugin-side capability unchanged).

## Capabilities

### New Capabilities
- `auto-update`: gentle, bounded scheduled refresh of chapter lists for barely-read library titles — interval knob, batch cap, attempt tracking, backoff, manual-refresh integration.

### Modified Capabilities
- `manga-detail-page`: the refresh path stamps the auto-update attempt tracker; the existing manual refresh button remains the on-demand path. (`settings-page` and `cli-config` unchanged: both already render/dump every registry knob generically, so registering `[update] interval_days` flows through without a spec change. `library-save` unchanged: auto-update only reads `read_ch` and reuses the refresh path.)

## Impact

- `src/config/registry.hpp`, `src/config/config.hpp` — new `update.interval_days` knob (Int, min 1, default 1).
- `src/store/` — new `update_attempts` table + migration + typed accessors; attempt recording wired into `/api/refresh`.
- `src/reader/` or new `src/update/` — scheduler (thread) with interval + jitter, batch selection query (saved_manga `read_ch <= 1`, oldest `last_attempt_at` first, cap 10).
- `src/http/http_api_title.cpp` — `/api/refresh` stamps an attempt row after the refresh completes (success or failure).
- `templates/detail*.mustache` — no new button needed; existing refresh button semantics are reused.
- `templates/settings.mustache` / settings renderer — the new knob appears automatically once registered.
- `openspec/specs/auto-update/spec.md` (new), `settings-page`, `manga-detail-page`, `cli-config` delta specs.
- Tests: store unit tests for the attempt table and batch-pick query; config registry test for the new knob.
