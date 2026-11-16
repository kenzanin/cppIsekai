# Proposal

## Why

Adopting the goIsekai comix.to plugin requires browser-automation capabilities the current host cannot express. The reader page delivers its image URLs only through lazy client-side hydration: the plain `lightpanda fetch --dump html` dump renders the page slots but returns zero `<img>` tags (verified 2026-11-16), and no intercepted/decrypted client-side payload exists to lift. `host.http.*` can execute JavaScript in the page when told to, but nothing in cppIsekai does that today. Meanwhile the composer question — which browser driver the host should absorb — is settled by evidence in the accompanying OKF research record: lightpanda (already in use for the 4ucomics port) handles comix end to end; Chrome and a CDP transport are not needed yet.

## What Changes

- New capability `browser-host`:
  - `host.browser.call(tool, args_json)` — a single macro-tool exposed to Lua (and mirrored in the JS and WASM bindings) that forwards one MCP tool call to an already-running lightpanda instance and returns the tool's content text. Tool surface: `goto`, `evaluate`, `waitForState`/`waitForSelector`/`waitForScript`, `getCookies`, `session_new/list/close`, `login` — generic passthrough, not per-tool bindings.
  - `lightpanda mcp --port` (streamable HTTP, JSON-RPC) is (re)used via a plain-HTTP client built on the in-repo `fetch::sync` gate (`src/fetch/fetch.cpp`) — no HTTP-client dependency; the only new dependency is `reproc` for child supervision (see Impact).
  - Session lifecycle: lazy `initialize`, cached `Mcp-Session-Id`, 404 → re-init + one retry, single in-process mutex serializing all `goto`→`evaluate` sequences.
  - Daemon supervision: cppIsekai itself spawns `lightpanda mcp --port 8899 --host 127.0.0.1` as an in-process child via the reproc library. On boot it probes the configured URL first; a healthy already-running lightpanda is reused, not spawned. On child death it respawns at most once (then marks the backend dead); on shutdown it stops only the child it owns. `run-cppIsekai.sh` returns to spawning only the app. New ini knobs `lightpanda_url` (default `http://127.0.0.1:8899`) and `lightpanda_command` (default `lightpanda`, for absolute path when PATH is minimal). Loopback targets bypass the host-wide proxy rule.
- Plugin manifest sentinel: `image_referer: "none"` means "send NO Referer header" for `/cache/img` upstreams that return 403 on any Referer (verified against the comix CDN). Today the fallback derives the origin from `website`, which would 403 every comix reader image.
- comix plugin pages handler switches to the new `host.browser.call` (goto → scroll-harvest script → image URL list) and stops failing with "no pages".
- Lua plugin image cache paths unchanged; `/cache/img` plumbing unchanged except the Referer sentinel.

Non-goals:
- No apicore crypto/HTTP-reverse-engineering route for comix.
- No `cpp-mcp` or CMake/httplib/nlohmann_json package adoption (SSE-only client, 405 from lightpanda; zero need — everything the host does fits in ~150 lines on existing primitives).
- No CDP/WebSocket client, no Chrome wiring; revisit only if lightpanda breaks a future target.
- No `session-per-harvest` isolation in v1; single mutex, single session (assumption A1 recorded in design.md).

## Capabilities

### New Capabilities
- `browser-host`: A lightpanda MCP browser-automation service exposed to plugins as one JSON-RPC passthrough tool (`host.browser.call`) plus the daemon lifecycle, session key handling, proxy exemption for loopback, and the `image_referer: "none"` manifest sentinel that make headless-DOM-driven plugins adoptable (starting with comix pages).

### Modified Capabilities
<!-- none -->

## Impact

- Affected code (planned, not yet touched): `src/browser/mcp.{hpp,cpp}` + `src/browser/supervise.{hpp,cpp}` (new), `src/plugin/lua/lua_runtime.cpp` + `src/plugin/js/js_bindings.cpp` + `src/plugin/wasm/wasm_imports.cpp` (binding), `src/fetch/fetch.cpp` (loopback-proxy exemption helper), `src/http/http.cpp` + `src/plugin/plugin_core.cpp` (Referer sentinel), `src/config/*` (two ini knobs), `xmake.lua` (one xrepo dep), `run-cppIsekai.sh` (drops lightpanda management), `plugins_dev/lua/comix/main.lua` (pages), tests (`tests/test_fetch.cpp` or similar for the rpc client wrapper).
- Dependencies: one added — `reproc` (xrepo/xmake-repo, v14.2.7, MIT, CMake recipe, links `reproc++` + `reproc`). No other dependency enters; JSON-RPC stays on glaze.
- APIs: one new host primitive visible to plugins (`host.browser.call`); zero HTTP-route changes expected.
- Systems: app is self-supervising; systemd units unchanged (PATH note documented in README).
- Risk: lightpanda crash/absence degrades only plugins that call `host.browser.*` (fail-loud); default browsing paths unaffected.
