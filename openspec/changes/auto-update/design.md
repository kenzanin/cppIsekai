# Design

## Context

See proposal.md for motivation. The host is a single-process daemon (`src/main.cpp`) with `db_pool` (single sqlite writer), `plugin_pool`, and a Crow HTTP server. The existing manual refresh path (`/api/refresh/<plugin>/<slug>`) is the template for how any chapters fetch happens: plugin-pool call, envelope validation, `call_cached`-style write-through to `api_cache`, then `sync_saved_stats` to roll up `total_ch`/`read_ch`/`has_update` into `saved_manga`.

Constraints:
- sqlite_orm store is single-connection, db-worker only; plugin threads never touch sqlite directly (`storage.*` gate pattern). The scheduler must hand off store access the same way.
- Plugin calls can block on network; the scheduler must not stall the HTTP server.
- Config is registry-backed (`src/config/registry.hpp`); every knob flows through resolve/dump/save automatically.

## Goals / Non-Goals

**Goals:**
- One scheduler thread that wakes regularly, decides whether a run is due (interval + jitter since last run), and processes at most 10 eligible titles with ≥1s pacing.
- Attempt tracking shared by scheduler and manual refresh (one `update_attempts` table, one writer path).
- Backoff for rate-limited/failing titles, without new UI.
- Reuse the existing refresh machinery instead of building a second chapters path.

**Non-Goals:**
- No persistent scheduler state beyond `update_attempts`; last-run timestamp lives in `plugin_state` (like skip flags) or is derived (max of attempt timestamps).
- No notifications, no per-title opt-out UI, no detail re-fetch.
- No new config section beyond `[update] interval_days`, `[update] per_plugin_limit` and `[update] request_delay_ms`.

## Decisions

- **D1 — Scheduler as a dedicated thread, tick = 60s.** A thread wakes every minute, reads the registry value fresh (so `/settings` edits take effect without restart), checks `last_run_at` from `plugin_state` key `update/last_run` plus jitter; when due, it selects the batch and calls the refresh path sequentially with a ≥1s sleep between titles. Alternative: Crow timer — rejected, couples to the HTTP server lifecycle and complicates shutdown.
- **D2 — Per-plugin batch cap, not global.** Eligibility query groups by plugin and takes `LIMIT per_plugin_limit` per group (e.g. SQLite window function or per-plugin loop in C++). Every site is then worst-case bounded independently — a large mangadex library can never starve a small scanlation site out of the batch, and no run ever exceeds N requests per site. Alternative: global cap of 10 — rejected, couples quotas across unrelated sites and breaks the "sadar diri per sumber" goal. Knob: `[update] per_plugin_limit` (Int, min 1, default 10); revisit the value against real site behavior, not by feel.
- **D2a — Reuse `/api/refresh` internals via a shared function.** Extract the refresh body into a function (or small class) called by both the HTTP route and the scheduler. The HTTP route keeps returning HTTP semantics; the scheduler uses the outcome only. This guarantees "manual counts as an attempt" and "chapter-scope only" for auto-update without duplication. Alternative: HTTP self-request — rejected, fragile and needs a port/loopback.
- **D3 — Chapters-only for auto-update.** The scheduler calls a chapters-scoped variant of the shared refresh (skip the detail call), so one request per title instead of two. The `update_attempts` row records the outcome. `sync_saved_stats` runs as usual; `has_update` flips when the upstream chapter count grew past stored `total_ch`.
- **D4 — `update_attempts` table keyed by `(plugin, slug)`** with `last_attempt_at`, `outcome` (`ok|error`), `error` text, `consecutive_errors` (for backoff), `backoff_until` (unix ts). Manual refresh and scheduler both upsert. Batch query: eligibility join `saved_manga` (`read_ch <= 1`) `LEFT JOIN update_attempts`, `WHERE backoff_until <= now OR backoff_until IS NULL`, `ORDER BY last_attempt_at IS NULL DESC, last_attempt_at ASC`, and `LIMIT per_plugin_limit` **per plugin** (partition by plugin).
- **D5 — Backoff schedule.** Per consecutive error, `backoff_until = now + min(2^n, 72)` hours. Success resets `consecutive_errors = 0`. 429 is classified from the plugin envelope error text (`upstream status 429` if present); anything matching is treated as rate-limit, everything else as a generic error (same math). Manual refresh always bypasses the backoff filter but still stamps the row.
- **D6 — Pacing knob.** `request_delay_ms` is `Type::Int`, default `1000`, flag `--update-request-delay-ms`, env `CPPISEKAI_UPDATE_REQUEST_DELAY_MS`, ini `[update] request_delay_ms`, doc "Delay between auto-update chapter-list requests (ms, 0 = no delay)". Read fresh per tick like the other two knobs. Alternative: hardcoded 1s — rejected per user request; a pacing knob is the one value an operator of a fragile site most plausibly needs to tune.
- **D6a — Interval validation.** `interval_days` is `Type::Int`, doc "Auto-update check interval (days, min 1)", flag `--update-interval-days`, env `CPPISEKAI_UPDATE_INTERVAL_DAYS`, ini `[update] interval_days`, default `1`. Validation at config load clamps <1 → boot fails with the registry validation error path (consistent with other Int knobs), never silently coerced. `per_plugin_limit` is the same shape: `Type::Int`, default `10`, flag `--update-per-plugin-limit`, env `CPPISEKAI_UPDATE_PER_PLUGIN_LIMIT`, ini `[update] per_plugin_limit`, doc "Max chapter-list refreshes per plugin per auto-update run (min 1)".
- **D7 — Last-run bookkeeping in `plugin_state`.** Key `update/last_run` (unix ts) written at the end of each run; jitter drawn per run (`interval - rand(0..15min)`). No new table for scheduler state.
- **D8 — `has_update` only grows via the shared path.** Auto-update doesn't write library stats itself; it calls the same chapters-scoped refresh, so `sync_saved_stats` rolls stats up identically to manual refresh. A freshly-updated title gets `has_update = 1` through the existing mechanism.

## Risks / Trade-offs

- [Scheduler doubles plugin traffic] → capped at 10 titles/run, 1s pacing, per-title interval gate, backoff; worst case 10 chapter-list requests per interval per library.
- [Refresh path assumes db + plugin pools] → both are available to the scheduler (it shares `db_pool`/`plugin_pool` handles); store access always `on_pool(db, ...)`.
- [Chapter-list refresh still costs one upstream request per title] → accepted; it is the minimum viable freshness signal.
- [429 detection relies on envelope text] → fragile across plugins; acceptable since any consecutive-error backoff is the same math — 429 is just a fast path, not different behavior.
- [Jitter uses rand()] → seeded from time at boot; drift is harmless (scheduler is advisory, not precise).
- [Thread lifetime] → scheduler thread is joined on daemon shutdown before pools drain (order: stop scheduler → stop HTTP → drain pools).

## Migration Plan

- New table `update_attempts` created by the existing `store_core.cpp` migration pattern (additive `make_table` + column defaults).
- New `[update]` ini section optional; default 1 day applies with no config edits. Existing `data/cppisekai.ini` untouched.
- Rollback: revert binary; `update_attempts` rows are inert data, safe to leave.

## Open Questions

None — the backoff curve, pacing, and batch cap are user-specified or derived from the "sadar diri" goal.
