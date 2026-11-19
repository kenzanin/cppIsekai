# Tasks: hot-path-perf

## 1. P1 — batched overlay lookup (library hot path)

- [x] 1.1 Add `Store::title_meta_get_many(pairs)` in the title-meta unit, `OpTrace` scope
      (`#5463`), one prepared statement over the key pairs; declared in `src/store/store.hpp`.
- [x] 1.2 `home_context` (`src/http/http_pages.cpp`): replace the per-title
      `chosen_cover`/`chosen_title` pair with one `title_meta_get_many` call + single
      parse pass building both maps (`http_pages.cpp:90-96`).
- [x] 1.3 Store test: `title_meta_get_many` returns rows for existing pairs, skips
      absent ones, and matches `title_meta_get` values.
- [x] 1.4 Record median `GET /` wall time before/after (10× `curl -o /dev/null
      -w %{time_total}`) in 5.2; revert per D1 kill criterion only on regression.

## 2. P2 — lua http opts dedup

- [x] 2.1 Extract `HttpOpts` + `parse_http_opts(...)` (anon ns, `lua_runtime.cpp`);
      `fetch` and `fetch_body` consume it; delete the duplicated blocks
      (`lua_runtime.cpp:203-232`, `:270-299`).
- [x] 2.2 Plugin probe: lua `host.http.fetch` + `fetch_body` behaviour unchanged
      (status/body/headers shape; timeout + cookie + headers opts still honored) —
      enginetest one-shot, then restore `enginetest/main.lua` from git.

## 3. P3 — api_cache age prune

- [x] 3.1 `Store::cache_prune_expired()` — `DELETE ... WHERE fetched_at + ttl_s <
      now-7d` (D3), OpTrace scope, returns deleted count; declared in `store.hpp`.
- [x] 3.2 Call it in the scheduler tick immediately before `maintenance_vacuum_backup`;
      log `[backup] pruned N cache rows` only when N > 0.
- [x] 3.3 Store test: expired-beyond-horizon rows deleted; fresh rows and
      long-ttl (`ttl_s` 30 d) rows survive; count returned.

## 4. P4 — SavedDetailPut struct

- [x] 4.1 `struct SavedDetailPut` + `bool saved_detail_put(const SavedDetailPut&)` in
      `store.hpp`; remove the 8-arg overload (D4).
- [x] 4.2 Update the caller (`src/http/enrich_detail.cpp`) with designated initializers.
- [x] 4.3 Update `tests/test_store.cpp` saved-detail cases to the struct form
      (set-cover + empty-keeps-cover cases).

## 5. Verification

- [x] 5.1 `xmake -y` clean (no new warnings) + `xmake test` 19/19; template goldens
      untouched (`git status` shows no `tests/fixtures/golden` change).
- [x] 5.2 Paste P1 before/after medians here: before 0.114 s / after 0.029 s (10-run
      median, warm daemon).
- [x] 5.3 Live sanity: library `/` renders identical grid (covers, dim+blur, sort
      state), one plugin detail refresh still persists via `saved_detail_put`.
      Evidence: `GET /` 71 grid rows, `POST /api/refresh/mangafire/09129` 200 `{"ok":true}`
      after blanking its `saved_manga.cover` — cover restored to 76 chars (struct write path live).
      P2.2 measured on enginetest: `fetch` status 200 / body string / headers table / custom
      header + `accept` + `user_agent` + POST body all honoured, `fetch_body` returns string,
      `timeout_ms=800` against `/delay/3` yields status 0 (abort honoured).
