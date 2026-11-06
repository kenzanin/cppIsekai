---
type: reference
title: cppIsekai dependency API surface
description: "Per-dependency functions and entry points with include paths; header-verified, includes curl_easy_impersonate, SimpleIni GetLongValue and sqlite_orm escape/limit/pragma caveats"
tags: [api, dependencies, cpp]
generated: { by: agent/mcp, at: "2026-11-06T13:50:44Z" }
status: stable
---

Entry points per dependency; every name verified against vendored/system headers (not from memory).

## Crow - `#include <crow.h>` (system package crow 1.3.4, /usr/include)
- `crow::SimpleApp app;`
- `CROW_ROUTE(app, "/path").methods("POST"_method)([](const crow::request& req){ ... return crow::response{...}; })`
- `app.port(8080).multithreaded().run();`
- WebSocket: `#include <crow/websocket.h>`

## libsql / sqlite amalgamation - `#include <sqlite3.h>` (static, third_party/libsql, 3.47.0)
- `sqlite3_open_v2`, `sqlite3_prepare_v2`, `sqlite3_bind_*`, `sqlite3_step`, `sqlite3_column_*`, `sqlite3_exec`, `sqlite3_finalize`, `sqlite3_close`
- `sqlite3_libversion()`
- libsql fork MVCC: `BEGIN CONCURRENT` transactions

## sqlite_orm - `#include <sqlite_orm/sqlite_orm.h>` (header-only, third_party/sqlite_orm, v1.9.1, AGPL-3.0)
- `make_storage(path, make_table(...), make_column(..., primary_key().autoincrement(), unique()), foreign_key(&T::fk).references(&P::id))`
- `storage.pragma.journal_mode(journal_mode::WAL)`; NO pragma.foreign_keys -- FK auto-enabled when schema declares foreign_key
- `storage.sync_schema()`, `storage.transaction_guard()` (commit/rollback RAII), `storage.insert(entity)`, `storage.count<T>()`
- queries: `select(columns(...), left_join<T>(on(c(a) == b)), group_by, order_by)`, `get_all<T>(where(...))`
- caveats: aggregate is `count(&E::member)` not `count<T>(member)`; no `is_true` (use `like(col, "%")`); use `limit(lim, offset(off))` -- standalone `offset()` serializer is incomplete; `like(...).escape(...)` needs `std::string("\\")` -- char binds as its NUMERIC value via field_printer<char>

## libvips - `#include <vips/vips.h>` (system shared, pkg-config vips)
- `vips_init(argv0)` / `vips_shutdown()`
- `vips_image_new_from_file`, `vips_image_write_to_file`
- `vips_thumbnail`, `vips_webpsave`, `vips_webpsave_buffer`, `vips_heifsave` (AVIF), `vips_jpegsave`

## sol2 - `#include <sol/sol.hpp>` (header-only; links lua5.4)
- `sol::state lua; lua.open_libraries(sol::lib::base);`
- `lua.safe_script("...")`, `lua["var"]`, `sol::function`
- binding: `lua.new_usertype<T>("T", "field", &T::field)`

## wasm3 - `#include <wasm3.h>` (static, third_party/wasm3)
- flow: `m3_NewEnvironment` -> `m3_NewRuntime` -> `m3_ParseModule` -> `m3_LoadModule` -> `m3_FindFunction` -> `m3_CallV` / `m3_Call`
- optional import linking: `m3_LinkLibC`
- cleanup: `m3_FreeRuntime`, `m3_FreeEnvironment`

## BS::thread_pool - `#include <BS_thread_pool.hpp>` (header-only)
- `BS::thread_pool pool(n);`
- `pool.submit(f, args...)` -> `std::future`, `pool.wait()`, `pool.wait_for(ms)`

## moodycamel::ReaderWriterQueue - `#include <readerwriterqueue.h>` (header-only, SPSC)
- `moodycamel::ReaderWriterQueue<T> q(capacity);`
- `q.enqueue(x)`, `q.try_dequeue(x)`, `q.size_approx()`

## Glaze - `#include <glaze/glaze.hpp>` (header-only, C++23)
- `glz::write_json(obj, buf)`, `glz::read_json(obj, buf)`
- automatic reflection from aggregate structs

## curl-impersonate - `#include <curl/curl.h>` (shared, third_party/curl-impersonate/lib, 8.1.1 BoringSSL)
- libcurl ABI: `curl_global_init`, `curl_easy_init`, `curl_easy_setopt`, `curl_easy_perform`, `curl_easy_cleanup`, `curl_version`
- fingerprinting: `curl_easy_impersonate(easy, "chrome116", 1)` -- NOTE: symbol exists in the .so (nm-verified) but NOT in system curl 8.23 headers; declare extern `CURL_EXTERN CURLcode curl_easy_impersonate(CURL*, const char*, unsigned);` or use include/ from the impersonate sources

## CLI11 - `#include <CLI/CLI.hpp>` (system package cli11 2.7.2, /usr/include/CLI)
- `CLI::App cli{"description"};`
- `cli.add_option("--port", var, "help")->envname("PORT");`, `->capture_default_str()`
- `CLI11_PARSE(cli, argc, argv);` -- macro prints parse errors and returns exit code

## SimpleIni - `#include <SimpleIni.h>` (header-only, third_party/simpleini, v4.27)
- `CSimpleIniA ini;` -- ctor(utf8, multiKey, multiLine), all default false
- `ini.LoadFile(path)` -> SI_Error (<0 = missing/error); `ini.SaveFile(path)` writes UTF-8 BOM
- getters are GetLongValue/GetValue -- NOT GetLong (build-error lesson); `ini.SetValue(section, key, value[, comment])`
- round-trip keeps whole-line comments, normalizes to `key = value`; trailing same-line comments are dropped

# Related Concepts
- [Dependency vendoring: system-shared first, minimal vendoring](../decisions/third-party-dependencies.md): API reference for the dependencies decided in this concept
- [Dependency vendoring: system-shared first, minimal vendoring](../decisions/third-party-dependencies.md): Dependency decisions reference updated with the sqlite_orm entry
