---
type: reference
title: "Verified layer map 2026-11-16 (runtimes, tables, cache v3, drift)"
description: "Fresh end-to-end survey of the shipped tree: request flow, per-layer real contents and counts, plugin matrix per runtime, cache layout v3 on-disk scheme, 18-knob config registry, and the doc/code drifts plus security smells found along the way."
tags: [code, architecture, reference, plugin, cache, store, config, drift]
generated: { by: agent/crush, at: "2026-11-16T00:45:45Z" }
status: stable
---

Survey taken 2026-11-16 against the working tree (graph 7743 nodes / 21872 edges, 55 C++ + 55 C files, 25 Lua, 9 HTML, 5 JS). Companion to references/code-layers, which is stale on counts and routes (see Drift).

## Request flow
browser -> templates/*.mustache (crow mustache, set_base in http.cpp:78) -> route (http_pages / http_api_*)
-> on_pool(db|plugin) (http_routes.hpp:33-56) -> Store (1 thread) | plugin Host (10 threads)
-> call_cached (stale.hpp:86-151) -> api_cache payload JSONB
-> Cache disk LRU -> curl-impersonate -> vips transcode -> GET /cache/img

Composition root: serve() in src/http/http.cpp:69 (gzip, static, pages, api, port, concurrency(4)), called from src/main.cpp:119.

## Real per-layer contents (counts as of this survey)
- http/: pages + /api split by domain into ~20 TUs. Envelope is NOT uniform: three shapes coexist -- plugin ABI {ok,data,error} (+ injected "cached"), HTTP wrappers {ok,data|sources|knobs|cleared}, and bare maps (/api/plugins, /api/position, /api/pages, /chapter/state).
- store/: 7 tables (plugin, title, plugin_state, reader_positions, saved_manga, title_meta, update_attempts, api_cache) + library_fts (FTS5, raw DDL). ONE long-lived sqlite_orm handle plus an ephemeral sqlite3_open per raw call. All Store access serialized through a 1-thread BS::thread_pool.
- plugin/: 3 runtimes behind one entry handle({api,method,params}); dispatch is a 3-arm if in plugin_call.cpp:139-147. Lua/sol2 (src/plugin/lua/lua_runtime.cpp), wasm3 (wasm_load.cpp / wasm_rt.cpp, 35 flat env.* imports), quickjs-ng (js_rt.cpp / js_bindings.cpp). Manifest = main.json (glaze, unknown keys ignored), optional config.json.
- cache/: layout v3, not v2. cover <ns>/cover/<sha1(url)-16>-<ratio4>.<ext>; page <ns>/manga/<slug>/ch<NNN>[-<id8>]/p<NNN>.<ext>. <ns> = slug of plugin display name. One global LRU keyed in memory, ensure() holds the mutex across the blocking fetch.
- reader/: pure next_window(chapters, pages_of, pos, skipped, limit=10) in window.cpp:14-38; Worker nudge-coalesces prefetch URLs; position merged via position_api.hpp:29-42.
- config/: registry has 18 knobs (not 10): server.port/proxy, plugin.plugins/fetch_timeout_ms/fetch_profile/plugin_timeout_ops, cache.cache_max_mb/image_format/cover_max_px, paths.library/log_file/data_dir/templates_dir/enrichment, log.level, update.interval_days/per_plugin_limit/request_delay_ms. Precedence default < ini < CPPISEKAI_* < explicit flag (opt->count()>0).

## Plugin matrix
Lua: 4ucomics, anisascans, atsumaru, enginetest, mangadex, mangafire, enrichment-mangadex, enrichment-mangaupdates.
JS: mangatown (pilot only). WASM: mangafox, mangafreak.
Live: mangafreak. Broken: mangafox reader end to end (needs c<ch> URL and the mobile page is JS-rendered, tasks.md:31-43). anisascans loads but upstream blocks search. 7 Cloudflare-blocked sites deliberately unported.

## Open work
openspec/changes/plugin-port-multi-runtime 6 done / 14 open; refactor-hygiene 0 / 21.

## Drift (doc lies, code is truth)
- cache.hpp:15-18 and openspec/specs/cache-layout-v3 still describe cover sidecar .json and plural covers/; code has no sidecar and singular cover/ (asserted gone in tests/test_cache.cpp:302). The living spec is openspec/specs/cache-layout.
- main.cpp:98 hardcodes cache root "data/cache", so [paths] data_dir never reaches the cache; same literal repeated in http_api_content.cpp:73,235 and http_api_state.cpp:172.
- main.cpp:67 comment "nothing queued yet" for db_pool is stale: every Store call hops onto it.
- config registry described as 10 knobs in AGENTS.md; it is 18.
- plugin_internal.hpp:118-119 and wasm_rt.hpp:15 point at "plugin.cpp", split into core/load/call long ago.
- references/code-layers still says seed 4 plugins x 12 titles, /history route, fetch=2 idle.
- cache api/ sha pool (cache.cpp:123) and the .json exclusion at cache.cpp:97 are dead in production.

## Fragility worth remembering before editing
- fetch-option marshalling exists in 4 places (lua_runtime.cpp:174-245, :247-303, plugin_load.cpp:26-108, partial 4th at lua_runtime.cpp:309-341); a new option must touch all.
- wasm imports do hand-rolled slot arithmetic with no arity/bounds check; a wrong sig string in kImports is UB, not a link error.
- Lua sandbox gap: luaopen_debug registered (lua_runtime.cpp:40) despite bridge.hpp:32-33, and io.lines() opens arbitrary absolute paths (:108-122).
- Duplicated shaping: saved-row 12 keys in http_pages.cpp:104-125 vs http_api_library.cpp:19-51; chapter-cache dir scan in http_api_content.cpp:69-102 vs http_api_state.cpp:170-210.
- br_asset serves .br bytes without Accept-Encoding negotiation (http_pages.cpp:249-256).
- Unauthenticated surface, fine only while bound to localhost: /cache/img is an arbitrary-URL proxy (http_api_content.cpp:183), POST /config has no CSRF, /admin/restart forks inside a handler (http_api_admin.cpp:326).

# Related Concepts
- [Source layer map and rules (src/<scope>/, .mustache templates)](../references/code-layers.md): Fresh 2026-11-16 survey; supersedes the stale plugin counts, /history route and pool sizing in this concept
- [src/http compile gotchas: global-scope helpers in api_ctx.hpp; crow::response is move-only](http-layer-gotchas.md): Reconfirms the precompressed-asset trap; br_asset still does not negotiate Accept-Encoding
