# Design

## Context

The host already links sol2 + Lua 5.4, wasm3, and curl-impersonate; `Store` is the only SQL gate; config loads via SimpleIni with precedence `--flag > env > ini > default` (see proposal.md for motivation). Plugin design decisions were locked in OKF (`architecture/plugin-system`, `architecture/normalization-contract`) — this change implements their first slice.

## Goals / Non-Goals

**Goals:**
- One loader + one binding (Lua) that later runtimes slot into without contract changes
- The mangadex plugin working end-to-end: search/detail/chapters into the UI
- Safe-by-default execution (sandbox + timeout) even though plugins are currently self-authored

**Non-Goals:**
- wasm/JS/micropython bindings (contract supports them; bindings come later)
- `dom.select` (lexbor) — mangadex is a JSON API; add when an HTML-scraping plugin exists
- Plugin marketplace/trust model beyond the sandbox
- Import UX/dedup UI (dedup decision recorded; lands with the search page)

## Decisions

**1. Data-contract ABI over native signatures.** Host calls `{"api":1,"method":...,"params":...}` (JSON text), plugin answers `{"ok":...}`. Alternative considered: per-runtime native function tables — rejected because adding a runtime would change the host, violating the flexibility requirement. Wire format stays JSON text; jsonb is DB-side storage only (see `architecture/plugin-system`).

**2. Lua binding shape: single entry function per plugin.** `main.lua` must define `handle(req_table) -> res_table` (sol2 converts tables natively). The host builds the envelope table, calls `handle`, serializes the result. Alternative: one global function per method (`search`, `detail`) — rejected; envelope dispatch keeps the contract runtime-agnostic and lets `main.json` stay free of method maps.

**3. One Lua state per plugin, created at load, reused across calls.** Cheap (mangadex calls are network-bound), keeps plugin globals meaningful between calls. A crashed/timeout state is recreated before the next call. Alternative: fresh state per call — safer but re-executes the whole chunk every call.

**4. Sandbox = stdlib whitelist at state creation + instruction hook.** `luaopen_*` only for base/string/table/math/json + a shimmed `io` (lines/read only); `os`/`debug`/`package` never opened; `lua_sethook(L, ..., LUA_MASKCOUNT, n)` checked against a per-call budget from `.ini` (`plugin_timeout_ops`, default a few million instructions). Timeout/error unwinds via `lua_error`, caught by `sol::protected_function_result`.

**5. `http.fetch` implemented as a blocking call on the calling worker.** Plugin calls run on a dedicated plugin pool (not the db or ui pool), so a slow fetch never blocks rendering. Timeout uses curl's own `CURLOPT_TIMEOUT_MS` (dual guard with the Lua instruction hook: hook catches CPU loops, curl catches network hangs). Profile maps to `curl_easy_impersonate(easy, profile, 1)` via the extern declaration (symbol exists in the `.so`, missing from system curl headers — see `references/dependency-api`).

**6. Manifest parse with glaze, loader errors isolated.** Each plugin folder is loaded independently; a failure logs a warning with the folder name and the next folder proceeds (spec: broken plugin doesn't break the host). Scan order = `std::filesystem::directory_iterator` order, first wins on id conflict.

**7. Plugin registration in DB via Store.** Loader upserts `(id, name, version, capabilities)` into the `plugin` table through `Store` methods; titles carry `plugin_id` (column exists) so imported manga reference their source plugin. Reload endpoint re-runs the same loader path.

**8. Logo route with static-type content type.** `/plugin/<id>/logo` reads the logo file from the plugin dir on demand (small files, no cache needed yet), content-type by extension (png/svg/jpeg/avif); 404 when absent.

## Risks / Trade-offs

- [JSON serialization per call] → negligible for network-bound scrape flows; revisit only if a plugin does high-frequency micro-calls
- [Instruction-count timeout is CPU-based, not wall-clock] → curl timeout guards the network half; the pair covers hangs and hot loops
- [Blocking fetch on plugin pool] → pool is sized separately (start at 2); fetches cannot starve UI or DB workers
- [MangaDex API changes] → plugin-level failure, isolated to `ok:false` responses; host behavior unchanged
- [First-wins id conflict is scan-order dependent] → acceptable until a trust model exists; warning makes it visible

## Migration Plan

No existing plugin consumers to migrate. Rollback = remove `src/plugin/` wiring; `Store` plugin-table upserts are additive. Seed plugins (Asura/Flame/etc. rows) remain for UI development until real imports displace them.

## Open Questions

(deferred deliberately — none affect this slice) wasm binding shape; plugin config schema in `main.json` beyond identity; concurrent manifest-driven pool sizing.
