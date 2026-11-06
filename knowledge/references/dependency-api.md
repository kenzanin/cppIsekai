---
type: reference
title: cppIsekai dependency API surface
description: "Per-dependency functions and entry points with include paths; header-verified, includes a curl_easy_impersonate prototype caveat"
tags: [api, dependencies, cpp]
generated: { by: agent/mcp, at: "2026-11-06T08:11:58Z" }
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

# Related Concepts
- [Dependency vendoring: system-shared first, minimal vendoring](../decisions/third-party-dependencies.md): API reference for the dependencies decided in this concept
