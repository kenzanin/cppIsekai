---
type: reference
title: cppIsekai dependency API surface + MangaDex auth resolution
description: "Per-dependency functions and entry points with include paths; header-verified, includes curl_easy_impersonate, SimpleIni GetLongValue and sqlite_orm escape/limit/pragma caveats"
tags: [api, dependencies, cpp, mangadex-auth]
generated: { by: agent/mcp, at: "2026-11-07T00:50:27Z" }
status: stable
---

cppIsekai dependency API surface (header-verified; curl-impersonate calls go through this layer).

== curl-impersonate (third_party/curl-impersonate/lib/libcurl-impersonate-chrome.so.4.8.0; headers: system curl/8.23)
- Extern declare: `extern "C" CURLcode curl_easy_impersonate(CURL* c, const char* target, unsigned int options);` (NOT in system headers; present in the .so, symbol verified). Call AFTER easy init / before perform; target like `chrome131` (`curl --impersonate list` from the chrome binary).
- Standard libcurl: curl_easy_init/setopt/perform/cleanup, CURLOPT_URL, CURLOPT_WRITEFUNCTION/WRITEDATA, CURLOPT_TIMEOUT_MS, CURLOPT_USERAGENT, CURLOPT_HTTPHEADER (slist), CURLOPT_POSTFIELDS (form body), CURLOPT_FOLLOWLOCATION.

== libvips (system 8.18.7; pkg-config vips)
- Init once in main(): `if (VIPS_INIT("cppisekai")) return 1;`
- Entry: `vips_thumbnail(...)` family (thumbnail_* ops), `vips_image_new_from_file` (any format incl. webp/avif with delegates), `vips_image_write_to_file`, `vips_image_write_to_buffer` (to memory), option strings like `[access=sequential]`.
- Link: `pkg-config --libs vips`; glib/pango/rsvg come transitively.

== wasm3 (vendored source third_party/wasm3/src -> build as static lib)
- C API from wasm3.h: m3_NewEnvironment, m3_NewRuntime(runtime, stackBytes, userData), m3_ParseModule(env, buf, size), m3_LoadModule(rt, mod), m3_LinkLibC(module) / m3_LinkLibWasmSyslib(module), m3_FindFunction(&f, rt, "name"), m3_CallV/Call/CallArity, m3_GetResults/GetArgv, m3_FreeRuntime, m3_FreeEnvironment. WASM binaries must be compiled with stack-bytes headroom (default runtime stack ~64KB).

== sol2 + Lua 5.4 (sol2 vendored v3.5.0-1-g9190880; system lua54 5.4.9, include /usr/include/lua5.4)
- sol::state (owns lua_State), state.open_libraries(sol::lib::base, string, table, math, ...); state.script / state.safe_script; state[name]=..., state[name](args) call; sol::table t = state.create_table(); usertype<T>: state.new_usertype<T>("T", "member", &T::m). Lua 5.4 integer/float split supported. Sandbox pattern: fresh lua_State + open only whitelisted libs + lua_sethook(L, hook, LUA_MASKCOUNT, N) instruction budget + pcall all entries.

== sqlite_orm (vendored v1.9.1, third_party/sqlite_orm/include) over libsql amalgamation (third_party/libsql/sqlite/sqlite3.c built as object into target)
- make_storage(path, make_table(...)), sync_schema(), pragma.journal_mode(WAL); pragma.foreign_keys NOT settable (FK enforced via schema decl).
- Queries: select(columns(...), where(...), order_by(...), limit(n), offset(o)); count(member) not count<T>(); multi-arg like: like(col, pattern).escape(std::string("\\")) - a bare char prints as number -> `ESCAPE 92` bug; limit(lim, offset(off)) combined overload when both present; ternary needs same branch types (c(&X::a)==v both sides); count aggregate needs sqlite_orm::count qualifier inside columns().

== glaze (vendored, third_party/glaze/glaze.hpp)
- glz::read<glz::opts{}>(obj, json_string) / glz::write<glz::opts{}>(obj) -> std::string; reflect on aggregates by default; glz::json_t for dynamic JSON (accessor: j["key"].get<std::string>(); array iteration).

== spdlog (system 1.17.0 shared, SPDLOG_FMT_EXTERNAL)
- spdlog::default_logger_raw / basic_logger_mt(name, path); sinks: stdout_sink_mt, rotating_file_sink_mt, ringbuffer_sink_mt (retrieves formatted log msgs via last_formatted()); custom sink: subclass base_sink<std::mutex>, override sink_it_(view) + flush_(); queue via spdlog::details::async_msg + own thread, or spdlog::async_factory with queue size.

== crow (system 1.3.4+; single header /usr/include/crow.h)
- SimpleApp app; CROW_ROUTE(app, "/path/<param>"), CROW_WEBSOCKET_ROUTE(app, "/ws/<room>") with .onopen/.onmessage/.onclose; app.port(n).multithreaded().run(); app.stop() for graceful shutdown. Response helpers: crow::response{body}, .set_header, crow::json::wvalue / mustache::context for templating. Mustache: crow::mustache::load_text(name, string) then tmpl.render(ctx) - partials via `{{> partialname}}` resolve through the SAME loader.

== SimpleIni (vendored v4.27+9190880-equivalent tag fd6db69, third_party/simpleini/SimpleIni.h)
- CSimpleIniA ini; ini.SetUnicode(); ini.LoadFile(path)/LoadData(mem, size); GetBoolValue(section, key, default), GetLongValue, GetValue(section, key, default); SetLongValue/SetValue/Delete(section, key, clear_comment); SaveFile(path). Comment preservation verified in practice; optional MultiKey true for repeated keys.

== CLI11 (system 2.7.2; #include <CLI/CLI.hpp>)
- CLI::App app{desc}; app.add_option("-p,--port", port, "")->check(CLI::Range(1,65535)); app.add_option("-c,--config", path)->envname("CPPSEKAI_CONFIG")->check(CLI::ExistingFile); CLI11_PARSE(app, argc, argv) or app.parse/exit.

== BS::thread_pool (vendored) / readerwriterqueue (vendored)
- BS::thread_pool<TP::opt::none> pool(n); pool.detach_task([]{...}), pool.submit_task(f) -> future; moodycamel::ReaderWriterQueue<T> q(cap); q.enqueue/emplace, q.try_dequeue(item) - single producer/consumer fast path.

== MangaDex auth (resolved 2026-11-06; docs source tmp/mdapi = mangadex-pub/mangadex-api-docs official GitLab mirror)
- Token endpoint https://auth.mangadex.org/realms/mangadex/protocol/openid-connect/token (Keycloak).
- `client_credentials` is for SERVICE clients only: our personal client returns 401 {"error":"unauthorized_client","error_description":"Client not enabled to retrieve service account"} - verified live. This is expected, not a config error.
- Personal client auth: password grant (username+password+client_id+client_secret -> access_token + refresh_token) for bootstrap, then refresh_token grant thereafter. No browser/authorization-code dance needed.
- Plugin design: config section [plugin.mangadex] in ini -> Lua global config; ensure_token(): cache -> refresh_token grant -> password grant -> nil (anon fallback, never fail the call over auth). 401 -> clear cache, re-auth once, retry once. `curl --data-urlencode` form POST; Content-Type application/x-www-form-urlencoded.

# Related Concepts
- Related to [Dependency vendoring: system-shared first, minimal vendoring](../decisions/third-party-dependencies.md)
