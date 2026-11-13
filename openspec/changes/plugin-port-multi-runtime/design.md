# Design: plugin-port-multi-runtime

## Context

cppIsekai plugin ABI is JSON-text-in/out with a single host wrap point (`call_cached` in `src/http/http.cpp`). Host tools: `http.fetch`, `storage.*`, `log.*`, `crypto.*`. Lua (sol2) is the only runtime wired today; JS (quickjs-ng) and WASM (wasm3) loaders exist as stubs/planned per `decisions/plugin-port-js-wasm`.

goIsekai corpus (`~/tmp/goIsekai/examples/plugins/`) has ~40+ Lua sites, ~40 JS sites, plus wasm/native/yaegi. cppIsekai will take Lua + JS + WASM only. Each plugin keeps its own `main.json` manifest and `main.<lang>` entrypoint under `plugins_dev/<lang>/<site>/`, deployed by symlink/copy into `data/plugins/` per `decisions/plugin-dev-layout`.

## Goals / Non-Goals

- Goals: port every non-Cloudflare-blocked site from goIsekai into cppIsekai across Lua/JS/WASM; per-runtime host-tool bindings; HAR-verified against real traffic; surface host-tool gaps per runtime.
- Non-Goals: Go native plugins, yaegi, Cloudflare-blocked sites, host `.ini` plugin sections, merging goIsekai's own OKF bundle.

## Decisions

**D1. Three runtimes, one ABI contract.**
All runtimes expose the same JSON-text-in/out exports: `search_manga`, `get_manga_detail`, `get_chapter_list`, `get_page_list`, optional `get_genres`, plus a `PLUGIN`/manifest table. The host `call_cached` wrapper is shared; only the language binding differs per runtime.

**D1a. Host-tool parity gap (adaptation, not copy).**
goIsekai Lua plugins use `host.text.*`, `host.regex.*`, `host.http.get_body`, `host.http.post_body`, `host.plugin.param(...)`. cppIsekai exposes `host.http.fetch(url, opts)`, `host.html.select/attr`, `host.re2.find/match`, `host.url_encode/decode`, `config.get(key)`, `host.storage`, `host.json`, `host.log`, `host.now`, `host.crypto.vrf_sign`. Porting a goIsekai plugin means adapting its helper layer to cppIsekai's host tools — not copying verbatim. Document each gap per plugin in its `main.lua`/`.js`/`.wasm` header comment.

**D2. Lua = existing sol2 path; add remaining goIsekai Lua sites.**
5 already ported (mangafire, atsumaru, mangadex, enrichment-mangadex, enrichment-mangaupdates). Add the rest from `~/tmp/goIsekai/examples/plugins/lua/` minus Cloudflare-blocked. Toolchain: sol2 + system lua54 (pkgconfig), per `CONSTRAINTS#sol2-lua-version` (sol2 3.5.0 rejects Arch lua 5.5).

**D3. JS = quickjs-ng runtime.**
New `src/plugin/js/` loader binding host tools into quickjs-ng global scope. Plugin entrypoint: `main.js`. Host tools exposed as a `host` global object mirroring Lua's `host.*`. Version: quickjs-ng 0.15.1 (already xrepo-pinned).

**D4. WASM = wasm3 runtime, toolchain wasi-sdk OR odin.**
New `src/plugin/wasm/` loader (wasm3 already vendored/xrepo). Plugin entrypoint: `main.wasm`. Build toolchain: wasi-sdk preferred; odin accepted as alternative per user note. Each wasm plugin compiled to a `.wasm` module under `plugins_dev/wasm/<site>/`.

**D5. Site assignment to runtime (1 site = 1 language, no duplication).**
Each goIsekai site is ported to exactly ONE of Lua/JS/WASM — never duplicated across runtimes. Assignment:
- Sites with simple JSON API + no WASM-specific need → Lua (simplest, already working)
- Sites needing heavy DOM/JS parsing or existing JS plugin logic → JS (quickjs-ng)
- Sites with existing wasm3 modules in goIsekai `wasm/` → WASM
- Cloudflare-blocked sites → skipped (listed, not ported)

Pilot first: one site per language (1 Lua, 1 JS, 1 WASM) as smoke test, then bulk port the rest.

**D6. Verification = HAR replay.**
Each ported plugin verified against `~/tmp/goIsekai/tmp/*.har` + `~/tmp/goIsekai/tmp/har/` real traffic. No live site dependence for CI.

## Risks / Trade-offs

- [quickjs-ng host bindings new surface] → JS loader already exists (`src/plugin/js/js_bindings.cpp`); start with one site smoke test before bulk port.
- [wasm3 + wasi-sdk/odin toolchain complexity] → wasm3 loader already exists (`src/plugin/wasm/wasm_imports.cpp`); toolchain choice (wasi-sdk vs odin) recorded in Phase 3 task 3.1.
- [Cloudflare detection] → some goIsekai sites may be blocked; skip and document rather than fight cf.
- [Host-tool gaps per runtime] → expected; documented per runtime as gaps surface.

## Migration Plan

Phase 1 (Lua): bulk-add remaining goIsekai Lua sites not already ported, skipping Cloudflare-blocked.
Phase 2 (JS): wire quickjs-ng loader + host bindings; port one smoke site; then bulk JS ports.
Phase 3 (WASM): wire wasm3 loader; pick wasi-sdk or odin; port one smoke site; then bulk WASM ports.
Phase 4 (Verification): HAR replay for every ported plugin; document gaps per runtime.
