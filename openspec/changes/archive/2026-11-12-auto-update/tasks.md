# Tasks

## 1. Config knob `[update] interval_days`

- [x] 1.1 Add registry entries in `src/config/registry.hpp`: `[update] interval_days` (flag `update-interval-days`, env `CPPISEKAI_UPDATE_INTERVAL_DAYS`, default `1`, type `Int`, doc "Auto-update: days between library chapter-list refresh runs (min 1)") and `[update] per_plugin_limit` (flag `update-per-plugin-limit`, env `CPPISEKAI_UPDATE_PER_PLUGIN_LIMIT`, default `10`, type `Int`, doc "Max chapter-list refreshes per plugin per auto-update run (min 1)"). Verify: `--dump-config` prints both knobs with doc comments; `test_config` passes.
- [x] 1.2 Enforce bounds on load/save: `interval_days`/`per_plugin_limit` < 1 and `request_delay_ms` < 0 (and any non-integer) are rejected with the existing Int validation-error path, never silently clamped. Verify: config test covers `interval_days = 0` rejected, `= 2` accepted, `per_plugin_limit = 0` rejected, `= 5` accepted, `request_delay_ms = -1` rejected, `= 0` and `= 2500` accepted.

## 2. Store: `update_attempts` table

- [x] 2.1 Add `UpdateAttemptsEntity` (plugin, slug, last_attempt_at, outcome, error, consecutive_errors, backoff_until) + `make_table` entry in `src/store/store_internal.hpp`; migration in `src/store/store_core.cpp`; fresh boot creates the table. Verify: daemon boots on a scratch db; `sqlite3` shows `update_attempts`.
- [x] 2.2 Store API in `src/store/store.hpp`/`store_api.cpp`: `upsert_update_attempt(plugin, slug, outcome, error)` (success resets consecutive_errors/backoff_until), `update_candidates(now, per_plugin_limit)` (join `saved_manga read_ch <= 1`, `backoff_until <= now OR NULL`, never-attempted first then oldest `last_attempt_at`, LIMIT `per_plugin_limit` **per plugin** via window function/partition), `plugin_state` helpers for `update/last_run`. Verify: new doctest cases in `tests/test_store.cpp` cover per-plugin cap (2 plugins × 30 eligible → 10 each), ordering, backoff exclusion, reset-on-success, manual-vs-auto same table.

## 3. Shared refresh extraction + attempt stamping

- [x] 3.1 Extract the body of `POST /api/refresh/<plugin>/<slug>` in `src/http/http_api_title.cpp` into a shared `refresh_title(...)` callable from both the route and the scheduler (result enum + failed-method name). Verify: route behavior unchanged (both methods still refreshed, same envelope/502 semantics); full test suite green.
- [x] 3.2 Stamp `update_attempts` on every refresh outcome (ok|error with plugin/host error text), in both the route path and the scheduler's chapters-scoped variant. Verify: call refresh manually, check `sqlite3 data/library.db 'select * from update_attempts'`.
- [x] 3.3 Add a chapters-scoped refresh variant (skips `detail`, calls only `chapters`, still runs `sync_saved_stats` + write-through cache for chapters). Verify: scripted call against a test build touches only the chapters endpoint (log/network-or-cache miss check) and updates `total_ch`/`has_update` as expected.

## 4. Scheduler thread

- [x] 4.1 Create `src/update/scheduler.{hpp,cpp}`: dedicated thread, 60s tick, reads `update.interval_days`, `update.per_plugin_limit` and `update.request_delay_ms` fresh from the registry on each tick, due-check vs `update/last_run` + per-run jitter (≤15 min), selects the per-plugin-capped batch via 2.2, `request_delay_ms` pacing between titles, sequential refreshes, writes `update/last_run`, logs run summary at info. All store access via `on_pool(db, ...)`; plugin calls via `plugin_pool`. Verify: with `interval_days=1` and a seeded scratch db (read_ch 0/1/5 titles + one backed-off title), a manually triggered run checks the right 10..N titles, skips the backed-off one, rewrites `update/last_run`.
- [x] 4.2 Failure/backoff handling: consecutive error increments `consecutive_errors`, sets `backoff_until = now + min(2^n, 72)h`; success resets. 429 in the envelope error text takes the same path (no special-case behavior beyond classification). Verify: simulated consecutive failures (unknown plugin id) in a unit/integration test produce growing `backoff_until`; a success resets it.
- [x] 4.3 Wire into `src/main.cpp`: start scheduler after pools/store are up, stop it before draining pools on shutdown (join, no detached thread). Verify: daemon boots, logs scheduler-start line, and exits cleanly on SIGINT.

## 5. Settings / docs / e2e

- [x] 5.1 Confirm the settings page renders the new knob with no template changes (generic registry projection), and `/settings` shows `update.interval_days` editable with restart-free semantics; note the ini section in `data/cppisekai.ini` comment block only if the template documents sections. Verify: curl `GET /config` includes the knob; `/settings` page contains `interval_days`.
- [x] 5.2 E2E smoke: fresh scratch data dir, seed one saved title with `read_ch <= 1`, run with a short interval override, confirm exactly one chapters fetch per interval tick per eligible title (capped per plugin), `update_attempts` rows written, manual refresh via the detail page stamps the same table and exempts the title, and a 429/backoff title is skipped with the correct log lines. Verify: log output + `sqlite3` output captured in the session.

## 6. Integration check

- [x] 6.1 Full suite green (`xmake test` or `ctest --output-on-failure`), daemon boots from `data/cppisekai.ini`, settings save of the new knob round-trips, and no warnings on build. Verify: build + test + one settings round-trip via `curl POST /config` + `GET /config`.

## Workflow follow-up

- Archive the change after review; sync `openspec/specs/auto-update/` (new) and `manga-detail-page` (modified) into main specs on archive.
