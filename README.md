# cppIsekai

Local C++23 daemon.

## Build

```bash
xmake -y
xmake test
./run-cppIsekai.sh
```

System prerequisites (Arch): `sudo pacman -S libvips curl crow asio`.

## System dependencies (shared, not vendored)

| Lib | Version | License | Role |
|---|---|---|---|
| libvips | 8.18.7 | LGPL-2.1 | image processing (WebP/AVIF) |
| crow | 1.3.4 | BSD-3-Clause | micro web framework (HTTP/WebSocket) |
| asio | 1.38.2 | BSL-1.0 | async networking under Crow |
| curl (headers) | 8.23 | MIT-like | ABI headers for curl-impersonate |
| CLI11 | 2.7.2 (pacman `cli11`) | BSD-3-Clause | flag parsing (`--port`, `--config`) |

## Vendored `third_party/` (only what xrepo lacks)

| Lib | Pin | License | Extracted |
|---|---|---|---|
| [wasm3](https://github.com/wasm3/wasm3) | master `28ecb9a` | MIT | `source/*.c,*.h` (built static) |
| [curl-impersonate](https://github.com/lwthiker/curl-impersonate) | v0.6.1 prebuilt x86_64 | curl license (MIT-like; [text](https://github.com/lwthiker/curl-impersonate/blob/master/LICENSE)) | `libcurl-impersonate-chrome.so.4.8.0` (the `-ff` variant ships in same release) |

## Via xrepo (`add_requires` in `xmake.lua`, resolved versions locked in `xmake-requires.lock`)

| Pkg | Pin | Role |
|---|---|---|
| glaze | 7.9.0 | JSON (store/http serialization) |
| sol2 | 3.5.0 | Lua binding (plugin runtime) |
| thread-pool | 5.1.0 | BS::thread_pool worker pool |
| readerwriterqueue | 1.0.7 | SPSC queues |
| lexbor | 3.0.0 | HTML parse (plugin htmltext) |
| libzippp | 7.1 | CBZ export wrapper |
| lua | 5.4.8 | MIT | Lua 5.4 runtime (plugin VM) |
| spdlog | 1.17.0 | MIT | logging |
| zlib | 1.3.2 | zlib | crow HTTP compression |
| sqlite3 | 3.54.0 | Public Domain | store engine |
| utf8proc | 2.11.3 | MIT | Unicode NFKD tokeniser (library-dedup) |
| re2 | 2025.11.05 | BSD-3-Clause | regex (html text extraction) |
| quickjs-ng | 0.15.1 | JS plugin runtime |
| simpleini | 4.27 | ini config read/write |
| sqlite_orm | 1.9.1 | store ORM |

Vendored copies of the above were removed 2026-11-12; history in git.

## Notes

- `curl-impersonate` links as a shared lib; `BUILD_RPATH` points at `third_party/curl-impersonate/lib`; TLS fingerprinting via BoringSSL + nghttp2.
- Pin hashes = `git rev-parse --short HEAD` at vendoring time; the clone scratch lived in `tmp/vendor/` (gitignored).
- Port precedence: `--port` > `$PORT` > `[server] port` in `data/cppisekai.ini` (comments survive rewrites via SimpleIni) > 8090.

## Enrichment plugins

`data/enrichment/` holds metadata-only enrichment sources (MangaDex, MangaUpdates first); it is NOT `data/plugins/` and these plugins never appear as manga/scraper plugins
— they are not rendered on the Plugins page, are not stale-served on the scraper
path, and cannot request chapters/pages.

Dev copies live in `plugins_dev/lua/enrichment-*/` and are symlinked in the same
way `data/plugins/mangadex` points to `plugins_dev/lua/mangadex/`, e.g.:

```bash
cd data/enrichment
ln -sfn ../../plugins_dev/lua/enrichment-mangadex mangadex-enrichment
ln -sfn ../../plugins_dev/lua/enrichment-mangaupdates mangaupdates-enrichment
```

Do not commit fetched enrichment payloads; they are user data and are stored via
`title_meta` once the detail page feature ships. The daemon logs loaded
enrichment sources on boot. See `openspec/changes/detail-enrichment-fields/` for
the feature contract.
