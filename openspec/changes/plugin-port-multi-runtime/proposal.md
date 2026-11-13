# Proposal: plugin-port-multi-runtime

## Why

cppIsekai currently ships 5 Lua plugins (mangafire, atsumaru, mangadex, enrichment-mangadex, enrichment-mangaupdates) — all ported from goIsekai. goIsekai holds ~40+ more Lua sites plus JS/WASM/native variants in `~/tmp/goIsekai/examples/plugins/`. The user wants every site that isn't blocked by Cloudflare ported into cppIsekai, distributed across the three supported runtimes:

- **Lua** (sol2) — existing path, 5 plugins already live
- **JS** (quickjs-ng) — new runtime path, ~40 sites in goIsekai `js/`
- **WASM** (wasm3) — new runtime path, build via wasi-sdk or odin toolchain

This surfaces host-tool gaps per runtime explicitly (per `decisions/plugin-port-js-wasm`) and grows the site catalog without picking one runtime for everything.

## Scope

**In scope:**
- Port goIsekai Lua/JS/WASM plugins not blocked by Cloudflare into `plugins_dev/<lang>/<site>/`
- JS runtime (quickjs-ng) bindings for host tools (http.fetch, storage, log, crypto)
- WASM runtime (wasm3) bindings, toolchain: wasi-sdk or odin
- Per-site `main.json` manifest + `main.<lang>` entrypoint
- HAR-capture verification against real traffic

**Out of scope:**
- Cloudflare-blocked sites (skipped, listed in tasks)
- Go native plugins (`native/` runtime) — cppIsekai has no native plugin path
- yaegi runtime — not part of cppIsekai's three runtimes
- Host `.ini` plugin sections (plugins_dev + symlink deploy per `decisions/plugin-dev-layout`)

## Capabilities

### New Capabilities
- js-runtime: quickjs-ng plugin loader + host-tool bindings
- wasm-runtime: wasm3 plugin loader + host-tool bindings
- Per-runtime plugin ABI (JSON-text-in/out, same envelope as Lua)

### Modified Capabilities
- lua-runtime: add remaining goIsekai Lua sites to the catalog

## Impact

- Code: `src/plugin/js/`, `src/plugin/wasm/`, `plugins_dev/js/*/`, `plugins_dev/wasm/*/`, `plugins_dev/lua/*/` (new sites)
- Config: none new (existing `fetch_profile`, plugin dev layout unchanged)
- No ABI change; each runtime gets its own loader wrapping the same JSON-text-in/out contract
