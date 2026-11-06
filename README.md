# cppIsekai

Local C++23 daemon.

## Build

```bash
cmake -S . -B build -G Ninja
cmake --build build
./build/cppIsekai
```

System prerequisites (Arch): `sudo pacman -S libvips lua54 curl crow asio ninja`.

## System dependencies (shared, not vendored)

| Lib | Version | License | Role |
|---|---|---|---|
| libvips | 8.18.7 | LGPL-2.1 | image processing (WebP/AVIF) |
| lua54 | 5.4.9 | MIT | Lua runtime for sol2 |
| crow | 1.3.4 | BSD-3-Clause | micro web framework (HTTP/WebSocket) |
| asio | 1.38.2 | BSL-1.0 | async networking under Crow |
| curl (headers) | 8.23 | MIT-like | ABI headers for curl-impersonate |
| CLI11 | 2.7.2 (pacman `cli11`) | BSD-3-Clause | flag parsing (`--port`, `--config`) |

## Vendored `third_party/` (only what is needed, licenses copied along)

| Lib | Pin | License | Extracted |
|---|---|---|---|
| [sol2](https://github.com/ThePhD/sol2) | v3.5.0 `9190880` | MIT | `include/` |
| [BS::thread_pool](https://github.com/bshoshany/thread-pool) | master `bd4533f` | MIT | `BS_thread_pool.hpp` |
| [readerwriterqueue](https://github.com/cameron314/readerwriterqueue) | master `6867b56` | BSD (see `LICENSE.md`) | `readerwriterqueue.h`, `atomicops.h` |
| [glaze](https://github.com/stephenberry/glaze) | master `0a363d4` | MIT | `include/` |
| [wasm3](https://github.com/wasm3/wasm3) | master `28ecb9a` | MIT | `source/*.c,*.h` (built static) |
| [libsql](https://github.com/tursodatabase/libsql) sqlite amalgamation | `main` `libsql-ffi/bundled/src` | Public Domain (sqlite) | `sqlite3.c`, `sqlite3.h`, `sqlite3ext.h` (3.47.0, built static) |
| [curl-impersonate](https://github.com/lwthiker/curl-impersonate) | v0.6.1 prebuilt x86_64 | curl license (MIT-like; [text](https://github.com/lwthiker/curl-impersonate/blob/master/LICENSE)) | `libcurl-impersonate-chrome.so.4.8.0` (the `-ff` variant ships in the same release) |
| [SimpleIni](https://github.com/brofield/simpleini) | v4.27 `fd6db69` | MIT (`LICENCE.txt`) | `SimpleIni.h` -- INI read/write, comments preserved |
| [sqlite_orm](https://github.com/fnc12/sqlite_orm) | v1.9.1 | AGPL-3.0 (`LICENSE`) | `include/sqlite_orm/sqlite_orm.h` -- Store backend (Pimpl) |

## Notes

- libsql is embedded as a static amalgamation; swap to the shared system lib once `libsql` (AUR) is installed (see the `ponytail:` marker in `CMakeLists.txt`).
- `curl-impersonate` links as a shared lib; `BUILD_RPATH` points at `third_party/curl-impersonate/lib`; TLS fingerprinting via BoringSSL + nghttp2.
- Pin hashes = `git rev-parse --short HEAD` at vendoring time; the clone scratch lived in `tmp/vendor/` (gitignored).
- Port precedence: `--port` > `$PORT` > `[server] port` in `data/cppisekai.ini` (comments survive rewrites via SimpleIni) > 8090.
