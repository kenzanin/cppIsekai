# Proposal: hot-path-perf

Performance and call-site hygiene pass #2 (successor to `refactor-hygiene`, archived
2026-11-18). No requirement changes, no user-visible behaviour change — this change
carries tasks only and has no spec deltas.

## Why

Re-audit 2026-11-19, every item verified against source before proposing:

- **P1 — library render does 2N serialized store calls.** `home_context`
  (`src/http/http_pages.cpp:90-96`) calls `chosen_cover` **and** `chosen_title` once per
  saved title; each performs its own `store.title_meta_get(plugin, slug)` and its own glz
  parse of the same row (`src/http/enrich_chosen.cpp:171,191`). For N saved titles that is
  `2N` OpTrace-serialized store round-trips plus `2N` JSON parses per library view — with
  the live library at N=66, 132 mutexed queries on the hottest page, every view.
- **P2 — lua http opts block duplicated.** AFT dup census:
  `src/plugin/lua/lua_runtime.cpp:203-232 == 270-299`. `host.http.fetch` and
  `host.http.fetch_body` each carry the identical ~45-line sol-table option parse
  (`timeout_ms/profile/method/body/accept/user_agent/cookie/headers`); `get_json` /
  `post_json` already share `json_call`. Every future host.http option must be added
  twice per runtime — exactly the drift the ABI memory (#5306) warns about.
- **P3 — `api_cache` is never age-pruned.** The only deletion paths are plugin
  unlink/purge (`src/store/store_api.cpp:85,244`). Expired rows and unlinked plugins'
  rows accumulate forever (755 rows live now; `pages` payloads are ~100 KB blobs).
  The maintenance tick (`store_maintenance.cpp`) is the natural owner — it already
  holds the raw-handle VACUUM slot on a schedule.
- **P4 — `saved_detail_put` is 8 positional `std::string` params**
  (`src/store/store.hpp:270-273`). Adjacent same-typed args; the 7→8-arg migration
  already required a bulk callsite sweep (2026-11-18). One more field repeats that.

Audit context, for the record: dead code 0, unused exports 0, diagnostics clean, clone
density 1.8 % (top groups third-party). Layering was verified one-directional in
`refactor-hygiene` and has not regressed — **no structural/architecture change is
warranted**; this change is surgical.

## What Changes

- **P1** one batched `title_meta` fetch for the saved set; `home_context` derives the
  overlay cover/title maps from a single parse pass. `2N` store calls → `1`.
- **P2** file-local `parse_http_opts` in `lua_runtime.cpp` used by `fetch` and
  `fetch_body`. Lua ABI unchanged.
- **P3** `Store::cache_prune_expired()` — delete rows past `fetched_at + ttl_s` by a
  wide margin, called from the existing maintenance tick before the vacuum/backup.
- **P4** `saved_detail_put` takes a `SavedDetailPut` aggregate; single callsite + tests
  updated. Named fields kill the silent-swap class.

Non-goals: no PCH (kill-tested twice, `refactor-hygiene` D6), no further store/http
splits (A1/A2 done), no JS/WASM binding rework (JS dedupes at the service boundary
already; wasm shims are thin by construction), no new config knobs.
