# Tasks

## 1. RPC client foundation

- [ ] 1.1 Implement `mcp_call(tool_name, args_json)` in `src/browser/mcp.{hpp,cpp}` (JSON-RPC tools/call over `fetch::sync` plain-HTTP to `lightpanda_url`): lazy `initialize`, cache `Mcp-Session-Id`, mutex-serialized call, 404 → re-init + retry once, bounded hard timeout. Verify: small doctest or manual probe via a scratch Lua plugin returns the tool result (e.g. `evaluate` on `data:text/html,<title>ok</title>`).
- [ ] 1.2 Parse the tool-result envelope with glaze (`result.content[0].text`, fall back to raw JSON string) and surface unknown-tool/transport errors as failure strings, matching host-call conventions. Verify: failure path exercised with a bogus tool name; log line present.

## 2. Host binding

- [ ] 2.1 Lua: bind `host.browser(tool, args)` in `src/plugin/lua/lua_runtime.cpp` following the existing `host.crypto`/`host.text` binding shape; return string or nil+err. Verify: `xmake test` green; ad-hoc `/plugin/<test-plugin>/call` with `host.browser` works.
- [ ] 2.2 JS + WASM: mirror the binding in `js_bindings.cpp` and `wasm_imports.cpp` (same generic shape, no per-tool macros). Verify: existing quickjs + wasm test binaries still pass; JS/WASM binding table lists the new import or export.

## 3. Config and proxy scoping

- [ ] 3.1 Add ini-only knob `lightpanda_url` to `src/config/` (ini `[lightpanda] url`, env `CPPISEKAI_LIGHTPANDA_URL`, default `http://127.0.0.1:8899`). Verify: `--dump-config` output shows the knob with default; env var overrides.
- [ ] 3.2 Loopback exemption in the fetch gate: when the configured URL host is `127.0.0.1` / `::1` / `localhost`, requests skip the host-wide proxy. Verify: unit test asserts no `CURLOPT_PROXY` is set in the loopback case and is set for external hosts.
- [ ] 3.3 Add ini-only knob `lightpanda_command` to `src/config/` (ini `[lightpanda] command`, env `CPPISEKAI_LIGHTPANDA_COMMAND`, default `lightpanda`). Verify: `--dump-config` output shows the knob with default; env var overrides.

## 4. Image fetch sentinel

- [ ] 4.1 Plugin manifest: parse `image_referer` (accept any string; sentinel value `"none"`). Verify: schema parser experiment or doctest covers sentinel, absent, and other values.
- [ ] 4.2 `/cache/img` upstream fetch (`src/http/http.cpp`) and origin fallback derivation (`src/plugin/plugin_core.cpp`): when sentinel is `"none"`, send no `Referer` header and no origin fallback; otherwise unchanged. Verify: live fetch to `resources.manga.com` (or test CDN) with and without sentinel produces the documented status codes (200 both), and no Referer observed on the wire via log/proxy shim.

## 5. In-process supervision (reproc)

- [ ] 5.1 Add `reproc` to the build as an xrepo package (`add_requires("reproc v14.2.7", {configs={c++=true}})`-style + `add_packages`, links `reproc++`). Verify: clean `xmake` build green with zero new warnings under the clang toolchain; binary links reproc (`ldd`).
- [ ] 5.2 Implement `src/browser/supervise.{hpp,cpp}`: at boot, probe `${lightpanda_url}/mcp` via the loopback path (reuse-if-running, D12); healthy → adopt, spawn nothing, and never stop it at shutdown. Unhealthy/absent → spawn `[lightpanda_command] mcp --port <port> --host 127.0.0.1` as an app-owned child; poll readiness bounded ~5 s; on failure, kill child + abort boot with a clear error. Guard all child ops with the browser mutex; on app shutdown, gracefully stop only app-owned children. Verify: fresh boot spawns exactly one app-owned lightpanda; killing the app leaves no orphan; with an external lightpanda pre-running on 8899, the app logs `reusing external lightpanda` and spawns none.
- [ ] 5.3 Crash path (D13): inside `mcp_call`, connect-refused/timeout triggers ONE respawn + session re-init + one retry of the original call; a second failure marks the backend dead for process lifetime (verbatim log line) and subsequent calls fail fast. Verify (manual): boot, `pkill -x lightpanda`, reload comix — pages resolve after exactly one respawn; kill twice — backend marked dead, zero respawn storm, error surfaced on the next call; app restart recovers.
- [ ] 5.4 README: document the supervision model (app-owned child, reuse-if-running, one-respawn rule), the `lightpanda_command` absolute-path escape hatch for minimal PATH units, and drop any stale two-unit/launcher instructions from `run-cppIsekai.sh` docs. Verify: section renders cleanly; `run-cppIsekai.sh` spawns only the app.

## 6. Comix plugin adoption

- [ ] 6.1 comix `pages` handler: switch to `host.browser.call` — `goto` reader URL, `waitForState` (or timed wait), then `evaluate` a scroll-harvest script collecting `.rpage-page img` srcs; preserve existing `created_at`/order. Verify: headless parse of the handler against chapter 22.5 returns the 6 image URLs.
- [ ] 6.2 Manifest: set `image_referer: "none"` for comix; purge comix `api_cache` rows (host `call_cached` is local-first and would shadow the fix). Verify: `sqlite3 data/library.db "select count(*) from api_cache where plugin_id='comix'"` → 0.
- [ ] 6.3 Reload and verify end to end from the running daemon: `http://localhost:8090/read/comix/...` opens, `/api/pages` returns N>0 page URLs, each image URL fetches `image/webp`/`image/jpeg` via `/cache/img`. Verify: all six (or real N) reader images render in a headless screenshot; reader console clean.
- [ ] 6.4 If the pages emitted by the plugin lack `created_at`, that's a known pre-existing gap — confirm whether unrelated and leave untouched here. Verify: inspect one emitted pages payload and confirm field set matches the mainline manifest notes; record anything new in OKF.

## 7. Integration checks

- [ ] 7.1 Full unit suite green (`xmake test`) after all edits. Verify: zero warnings, 18/18 targets pass.
- [ ] 7.2 Boot smoke + weekly ritual: daemon boots (app-owned lightpanda via `pgrep -af 'lightpanda mcp'` shows exactly one child of the app), plugin list still shows `comix` enabled, `/view/manga/comix/...` renders, `/api/library` 200; write the OKF decision\*s for any new facts discovered during apply. Verify: all four responses show the documented macros fire (HTTP 200, correct payload).
- [ ] 7.3 docs: one-paragraph "Browser automation" section in the README covering ini knob, daemon expectation, loopback rule, sentinel semantics, one example `host.browser.call` harvest fragment. Verify: markdown lint or clean render.

## Workflow follow-up

- Archive `browser-host` after review; reconcile specs into the main bundle.
- Any JS-engine overview in `.superpowers/sdd/` gets refreshed only if the repo workflow requires it.
- If the harvest later migrates to session-per-harvest isolation (A1 revisit), file a follow-up openspec change rather than silently rewriting D3.
