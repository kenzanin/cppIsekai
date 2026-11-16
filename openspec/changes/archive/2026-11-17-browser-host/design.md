# Design

## Context

See `proposal.md` — Why. Key facts established empirically (2026-11-16, evidence recorded in OKF `decisions/comix-adoption-route-2026-11-16`):

- live chapter 22.5 of comix.to (`11727328-chapter-22.5`): `lightpanda fetch --dump html` renders six `.rpage-page` slots and zero `<img>` tags; `lightpanda run` with a scroll-into-view script + `document.querySelectorAll('.rpage-page img')` returns 6/6 URLs, deterministic across runs.
- comix API (`api/v1/chapters/<id>`) returns `{"e":"<base64>"}`; no JS-hooked plaintext was obtained, apicore crypto reimplementation has failed across two prior sessions; not adopted.
- comix CDN answers 200 image/webp with no Referer and 403 with any Referer; plugin manifest sentinel `image_referer: "none"` is required.
- `lightpanda mcp --port <p>` (streamable-HTTP, `POST /mcp` JSON-RPC) is present and verified live: `initialize` → 200 + `mcp-session-id: <session>` header; `tools/list` → 39 tools including `goto`, `evaluate`, `scroll`, `waitForSelector`, `waitForScript`, `waitForState`, `getCookies`, `session_new/list/close`, `login`.
- `GET /sse` on the same instance → **405 Method Not Allowed**; cpp-mcp ships only a legacy SSE client plus a stdio client.
- User decisions (2026-11-16): spawn + supervise the lightpanda daemon **inside the app** (not shell lifecycle); one respawn-then-dead on crash; and at boot, reuse a healthy already-running lightpanda instead of spawning a second one (never stop someone else's instance). Implemented via xrepo `reproc`.

## Goals / Non-Goals

**Goals**
- One `host.browser.call(tool, args_json)` passthrough binding in all three runtimes (Lua first, JS and WASM mechanical).
- Exactly one new xrepo dependency (`reproc`, MIT) for child-process supervision; everything else reuses `fetch::sync`, glaze, and the existing plugin-fetch helpers.
- comix plugin pages handler returns the real page list; `/cache/img` serves every page JPEG without a Referer; reader renders 6/6 images.
- cppIsekai spawns and supervises the lightpanda daemon itself (reproc++, in-process child, lazy crash recovery, graceful stop on shutdown); `run-cppIsekai.sh` goes back to spawning only the app.

**Non-Goals**
- No decision here changes curl-impersonate fetch semantics, proxy behavior for site fetches, or the `/cache/img` pipeline beyond the Referer sentinel.
- No browser automation beyond one bounded harvest per call; in-memory session state is not shared with enrichment.
- No `cpp-mcp`, CDP/WebSocket, or Chrome adoption (Design D2, D3, D4).

## Decisions

### D1 — External HTTP daemon instead of embedding a client library
**Alternatives considered:**
- `hkr04/cpp-mcp` (xrepo `cpp-mcp 2025.05.24`) — MIT, C++17, CMake + httplib + base64 + nlohmann_json; rejected: only a legacy SSE HTTP client exists (server-side supports streamable-HTTP, client does not) and `/sse` → 405 confirmed live; adds four dependencies and ~half-day build/verification risk for a two-call surface.
- `lightpanda mcp` stdio spawned by CPPISEKAI via `mcp::stdio_client` — viable, halves operational burden, but violates the agreed "lightpanda runs first as a daemon" deployment and couples the app lifecycle to a third-party CMake package. Deferred; revisit if we want per-request session isolation.
- `popen("lightpanda run <script>")` per harvest (Route A) — zero deps but hands every harvest to the shell with quoting exposure, ~1–2 s spawn per call, and forces the evaluate script to inline-shipper as a temp file. Rejected after we proved `mcp --port` exposes everything we need.
- **Chosen**: cppIsekai talks to a `lightpanda mcp --port 8899` instance over `POST /mcp` plain JSON-RPC. Footprint ~150 LOC (`src/browser/mcp.cpp`); no client library; the daemon is the app's own child by default (see D10) — no shell step, no systemd sidecar requirement.

### D2 — Plain JSON over HTTP, not Server-Sent Events
lightpanda answers `tools/call` with `application/json` when the client sends `Accept: application/json, text/event-stream` (verified). The client parses the envelope directly with glaze and reads `result.content[0].text`. SSE as a wire format is never parsed; if lightpanda ever flips response framing we fail loud and surface it (see Open Questions).

### D3 — One local session, one mutex
All browser call sequences share one in-process `std::mutex` and one cached `Mcp-Session-Id`. Concurrent harvests serialize; each sequence observes only its own page state because the mutex never releases mid-sequence. Session-per-harvest (via `session_new`) is a deferrable variant (see Open Questions / Assumption A1).

### D4 — Lazy init + single 404 retry
First browser call triggers `initialize` (store `mcp-session-id`); no startup coupling to the app boot sequence beyond the port wait. A 404 from any subsequent call frees the cached id, performs one fresh `initialize`, and retries the call once. A second consecutive 404 reports failure to the plugin (never loops).

### D5 — One generic `host.browser.call`, not per-tool bindings
Site authors write the recipe in the plugin (goto → waitForState/settled → evaluate(scroll harvest) → parse); the host stays stable as the MCP tool list evolves. Per-tool Lua bindings would triple the binding surface and track lightpanda internals for no behavior gain. JS and WASM mirror the same generic shape (`host.browser_call(tool, argsJson)`); no Lua-only special-casing.

### D6 — Loopback bypasses the host-wide proxy
The `fetch::sync` path applies the ini proxy to all requests today; lightpanda's URL targets loopback. `fetch::sync` gains a loopback-host exemption (`127.0.0.1`, `::1`, `localhost`) applied only for this use case. Site fetches are unchanged.

### D7 — CDP/Chrome rejected for now
`lightpanda mcp` exercises `evaluate`/`waitForState` natively and needs no long-lived socket; Chrome adds CI weight and process supervision the project does not want. Chrome remains a per-OS fallback in the lightpanda driver config if lightpanda ever breaks on a target — handled as a deployment concern, not a host concern.

### D8 — apicore crypto route rejected
Two prior sessions reconstructed `XB`, `Zj`, `wo`, `Q`, and material tables `Aj`/`Bj` without obtaining plaintext chapter JSON; the `w=f001f101` marker can change silently. Not adopted; the DOM harvest replaces it with ~6 lines of JavaScript.

### D9 — `image_referer: "none"` sentinel in the manifest
Not derived from `website`; explicit sentinel keeps backward compatibility (unknown atributes are ignored). Implementation touchpoints: `src/http/http.cpp` (Referer derivation for `/cache/img`) and `src/plugin/plugin_core.cpp` (fallback origin derivation) — both consult the manifest before setting the header; `"none"` means the fetch proceeds with no `Referer`.

### D10 — In-process supervision via reproc++, not shell lifecycle
**Chosen:** cppIsekai spawns `lightpanda mcp --port <p> --host 127.0.0.1` as a direct child of the daemon, spawned eagerly during app boot (before the HTTP port binds) so a dead browser backend fails fast and loud instead of on first harvest. All reproc operations funnel through one supervisor object guarded by the D3 mutex — no third thread, no duplicate `wait` calls.
**Alternatives considered:**
- Shell lifecycle (`run-cppIsekai.sh` owns lightpanda, verify + kill-on-exit): zero deps, but a mid-run crash persists silently — exactly the supervision the user asked to add — and process ownership is ambiguous (app can't tell whether it crashed). Rejected.
- Hand-rolled fork/exec + child waiter thread (~80 LOC, zero dep): Linux-only is fine here, but zombie reaping, graceful terminate→kill, and stdio hygiene come free in reproc; the user explicitly picked the library. Accepted as the recommendation for this project.
- systemd `Restart=` on both units: deployment-coupled, not expressible in the repo, rejected.
- **Package:** xmake-repo already ships reproc (`packages/r/reproc/xmake.lua`; MIT; default version v14.2.7; `add_deps("cmake")`; C++ wrapper default ON → links `reproc++` + `reproc`). Adding it is a one-line `add_requires`/`add_packages`; no vendored source, no patch.

### D11 — Config knobs live in the standard registry
`lightpanda_url` (ini `[lightpanda] url`, env `CPPISEKAI_LIGHTPANDA_URL`, default `http://127.0.0.1:8899`) and `lightpanda_command` (ini `[lightpanda] command`, env `CPPISEKAI_LIGHTPANDA_COMMAND`, default `lightpanda` — an absolute path for systemd environments with a minimal PATH). Both ini-only (no CLI flags; port setters would collide with `--port`). `--dump-config` renders them with the rest.

### D12 — Reuse-if-running: probe before spawn
At boot (before any spawn), the supervisor probes `<lightpanda_url>/mcp` through the loopback path (D6). Any parseable MCP response = healthy → adopt the instance: log "reusing external lightpanda", do NOT spawn, and do NOT stop it at shutdown. No healthy response → the app owns the spawn (D10). Adopted instances get a full re-init (fresh `Mcp-Session-Id`) on the first call; this also absorbs operator restarts of lightpanda.

### D13 — Crash detection is lazy on the call path; no watcher thread
Death is detected exactly where it is felt: inside `mcp_call`, a connect-refused or timeout triggers ONE transparent respawn (new child replaces the dead handle), a fresh session init (D4 path re-fits for free), and one retry of the original call. If the retry still fails, the browser backend is marked "dead" for the remainder of the process — subsequent calls fail fast, no restart storm, no race with `shutdown()`. There is deliberately no SIGCHLD/waiter thread; zombie reaping happens at child stop/shutdown where the spawn handle is already owned. Callers that want to distinguish "backend dead" from "site down" read the log line, not the call return value.

## Risks / Trade-offs

- [Risk] lightpanda hangs the harvest (`waitForState` never fires or `evaluate` blocks) → **Mitigation**: bounded per-tool `timeout` in the args the plugin passes; the host additionally enforces a hard `timeout_ms` on the RPC HTTP request (default 15 s, overridable per call later); on failure the plugin receives a normal fetch failure and the UI shows the existing "failed to load" path.
- [Risk] lightpanda resource leak under repeated `session_new` usage if future plugins misuse it → **Mitigation**: v1 never calls `session_new` (single implicit `initialize` session); misuse is confined to plugins opting into `session_*` explicitly; we document danger in the manifest section.
- [Risk] Stale session id after lightpanda restart → **Mitigation**: D4 shines exactly here; app restart passes no state to lightpanda, first call re-initializes.
- [Risk] Concurrency on the harvest (two users open two comix readers simultaneously) → **Mitigation**: D3 mutex queues them; harvest is ~1–2 s each locally — acceptable for a single-user local daemon. Session-per-harvest deferred (see assumption).
- [Risk] Two stacks owning JSON (glaze + any transitive binding JSONType) in the daemon → **Mitigation**: no third-party JSON is introduced; everything stays on glaze.
- [Risk] `reproc` CMake recipe (`add_deps("cmake")`) build friction on fresh caches → **Mitigation**: same pattern already proven in this project tree (glaze-style xrepo+CMake); if it fails, pin via `xmake f -c -y` purge as per xrepo-install memory note; never patch reproc itself.
- [Risk] lightpanda binary missing from the daemon's PATH (systemd user units) → **Mitigation**: `lightpanda_command` ini knob for an absolute path; the supervisor logs the exec failure verbatim at boot instead of half-starting.
- [Risk] Orphaned lightpanda when the app is SIGKILLed → **Mitigation**: orphan is detected by D12's reuse probe at the next boot and simply reused (their pid, our session); no leaked processes accumulate.
- [Risk] Parent writes to a dead child's stdio → **Mitigation**: the app never writes to child stdio; SIGPIPE is ignored at supervisor init per reproc guidance anyway.
- [Trade-off] lightpanda `mcp --port` is a single shared process → **Mitigation**: local-only daemon, trust boundary unchanged; `--host 127.0.0.1` default bind.

## Migration Plan

1. Implement host primitive + Lua binding behind nothing (no flag); plugin changes are backwards-free-flowing (pages handler change is inside the comix plugin).
2. Deploy locally via `run-cppIsekai.sh` (unchanged reporter; the app now self-supervises lightpanda).
3. systemd users: no unit change — the app owns the child; document the `lightpanda_command` PATH note in README.
4. Rollback: revert commit; orphaned lightpanda (if any) is detected and reused at next boot; kill it manually if discarded.
5. Cache: purge comix `api_cache` rows after the pages handler change (stale envelope shadows the fix), same as every prior plugin response-shape change.

## Assumptions (recorded; challenge before implementation if wrong)

- **A1** — One shared session with a single mutex is acceptable for the local single-user deployment; session-per-harvest isolation is not needed for comix pages. Revisit only if concurrent multi-user usage shows up in real operation.
- **A2** — lightpanda answers with plain JSON for `tools/call` on the versions we pin; if it ever switches to SSE framing, the RPC client fails loud (request/response mismatch) and this change documents how to parse the framing from the raw bytes.
- **A3** — `image_referer: "none"` is sufficient for the comix CDN; no cookie-aware replay is needed (cookie jar lives inside lightpanda, not the image fetch path).
- **A4** — `lightpanda` is on the PATH the daemon runs with (true locally); minimal systemd environments set `[lightpanda] command = /absolute/path`. The supervisor logs a clear error and aborts boot if the binary is not executable (fail-fast, not limp-along).

## Open Questions

- Whether `/sse` compat exists in some future lightpanda build (167+) that would let tools/call stream tokens back; harmless — we never request SSE.
- Whether the comix chapters endpoint (`api/v1/chapters/...`) starts returning plaintext for a different `w` value someday; no planned action.
