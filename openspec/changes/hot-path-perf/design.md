# Design: hot-path-perf

## D1 — P1 batched overlay lookup, single parse

`Store::title_meta_get_many(const std::vector<std::pair<std::string, std::string>>& keys)`
returns the rows for the saved set in one prepared statement
(`plugin + slug` IN-PAIRS; chunk if ever > 900 pairs — SQLite var limit, N≈10² today).
Opens with the `OpTrace` scope per #5463; read-only, no `TxGuard` (#5614 is for
explicit transactions).

`home_context` calls it once, parses each `chosen` JSON **once**, and fills both the
cover-map and title-map from that pass. `chosen_title` / `chosen_cover` stay for the
detail/detail-refresh paths (single-key callers); their bodies are untouched.

Kill criterion: median `GET /` wall time is recorded before/after (10 runs, curl
`%{time_total}`); revert only on regression, not on "imperceptible" — the win also
cuts lock contention on the shared connection, which the wall clock partially hides.

## D2 — P2 lua opts parse helper

Anonymous-namespace `struct HttpOpts { long ms; std::string profile, method, body,
accept, ua; bool cookie; std::vector<std::pair<std::string, std::string>> headers; }` +
`parse_http_opts(const sol::optional<sol::table>&, long default_ms)` in
`lua_runtime.cpp`. `fetch` and `fetch_body` call it; field defaults identical to
today (`method="GET"`, `cookie=false`, ms from conf). No Lua-visible change — the
`host.http.*` surface of #5306 is frozen; this is runtime-internal only.

## D3 — P3 prune horizon: expiry + 7 days, no knob

`DELETE FROM api_cache WHERE fetched_at + ttl_s < ?now - 7d`. Rationale:

- `stale.hpp` serves expired rows stale-while-revalidate; a **wide** horizon keeps that
  window practically intact — only rows ≥7 days past their own TTL are collected, i.e.
  data no live path can want.
- Per-row `ttl_s` is respected (plugin manifest `cache_ttl` overrides, #memory
  cache-TTL v2), so a plugin declaring a 30-day metadata TTL is never collected early.
- No config knob (YAGNI): the horizon is derived, not tuned; revisit only if a real
  workload needs it.

`Store::cache_prune_expired()` (OpTrace, plain DELETE — no TxGuard) is invoked by the
scheduler immediately before `maintenance_vacuum_backup`, logs `pruned N cache rows`.
Kill criterion: any stale-serve test regression → narrow the DELETE to rows of
unlinked plugins only.

## D4 — P4 `SavedDetailPut` aggregate

```cpp
struct SavedDetailPut { std::string plugin, slug, name, authors_json, artists_json,
                        tags_json, synopsis, cover; };
bool saved_detail_put(const SavedDetailPut& in);
```

Declared in `store.hpp` next to the old signature's slot; the sole production caller
(`enrich_detail.cpp`) and `test_store.cpp` switch to designated initializers —
field naming becomes compile-checked, ending the positional-swap class. Old 8-arg
overload is removed, not kept (one caller; dead overloads are the thing this change
deletes).

## D5 — Verification gate (whole change)

`xmake -y` zero-new-warnings, `xmake test` 19/19, template goldens untouched (no
`.mustache` change in this change), P1 timing recorded in `tasks.md` 5.2. Plugin
behaviour parities (#5362/#5363) not triggered — no plugin/runtime ABI change ships
(D2 is internal; D3 only deletes rows a live path re-fetches).
