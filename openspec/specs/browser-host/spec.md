# browser-host Specification

## Purpose
Lets plugins drive a real browser through the host — navigation, JavaScript evaluation, and waiting — so plugins can adopt sites whose content is materialized only by client-side code. Without the host embedding a CDP stack; exactly one small xrepo dependency (reproc) handles child-process supervision.

## Requirements

### Requirement: Browser tool passthrough
The host SHALL expose one plugin-accessible function (`host.browser.call`) that forwards a named MCP browser tool and a JSON `tools/call`-shaped argument object to the configured lightpanda MCP service, and returns that tool's result to the calling plugin. Supported tool names SHALL at least cover `goto`, `evaluate`, `waitForState`, `waitForSelector`, `waitForScript`, `getCookies`, `session_new`, `session_list`, `session_close`, and `login`.

#### Scenario: Successful evaluate
- **WHEN** a plugin calls `host.browser.call("evaluate", {script="return document.title"})` after an earlier `goto` call
- **THEN** the call returns an envelope the plugin can consume as text, and the browser session keeps the page loaded from the prior `goto`

#### Scenario: Unknown tool fails explicitly
- **WHEN** a plugin calls `host.browser.call` with a tool name the configured lightpanda does not expose
- **THEN** the host returns a failure the plugin can detect, and the host log records the tool name and the failure detail

### Requirement: Serialized multi-step sequences
The host SHALL serialize complete call sequences that depend on shared browser state (for example `goto` then `evaluate`) so concurrent plugin invocations never interleave browser actions across the shared session.

#### Scenario: Concurrent harvests do not interleave
- **WHEN** two plugin calls, each performing `goto` followed by `evaluate`, execute concurrently
- **THEN** the second sequence starts only after the first sequence finishes, and each observes its own page state

### Requirement: Session recovery on expiry
The host SHALL keep the lightpanda MCP session id in memory per process and SHALL recover exactly once from a 404-style session-expired response by re-initializing the session and retrying the original tool call.

#### Scenario: Session expires mid-session
- **WHEN** lightpanda reports the cached session id no longer exists (closed idle session) during a tool call
- **THEN** the host initializes a fresh session, retries that one tool call once, and either returns the result or reports the failure

### Requirement: Lightpanda unreachable surfaces as failure
The host SHALL report an explicit, loggable failure — not a hang — when the configured lightpanda URL refuses connections or times out, and plugin callers SHALL receive the ordinary failure signal used for other host calls.

#### Scenario: Daemon not running
- **WHEN** `host.browser.call` is invoked while lightpanda is not listening on the configured URL
- **THEN** the call fails within the configured timeout, the host log names the unreachable URL, and the plugin receives a failure it can branch on

### Requirement: In-process supervision of the lightpanda daemon
cppIsekai SHALL spawn `lightpanda mcp` as an in-process child process (via the reproc C/C++ library) unless an already-healthy lightpanda answers on the configured URL when the app starts, in which case it SHALL reuse that instance instead of spawning a second one. An instance the app did not spawn SHALL be left running when the app exits.

#### Scenario: Startup with nothing running
- **WHEN** cppIsekai boots and the configured lightpanda URL does not answer
- **THEN** the app spawns `lightpanda mcp --port <port> --host 127.0.0.1` as a child, waits for the URL to become healthy, and the HTTP port binds afterwards

#### Scenario: Startup with a reused instance
- **WHEN** cppIsekai boots and an existing healthy lightpanda answers on the configured URL
- **THEN** the app spawns no second child and the existing instance keeps running after the app exits

### Requirement: Single respawn on child death, then fail fast
On death of the child it spawned, the host SHALL respawn the lightpanda child once, re-initialize the MCP session, and retry the interrupted call exactly once; if that retry also fails, the host SHALL mark the browser backend dead for the remainder of the process and fail subsequent calls fast. On app shutdown the host SHALL gracefully stop only the child it spawned — terminate, then kill if the grace period elapses.

#### Scenario: Child crash mid-operation
- **WHEN** the spawned lightpanda dies (port refuses connection) during a `host.browser.call`
- **THEN** the app respawns it once, re-initializes the MCP session, retries the call once, and either returns the result or reports the browser backend as dead

#### Scenario: Restart storm prevented
- **WHEN** repeated child deaths keep the retry failing
- **THEN** after the single respawn attempt the backend is marked dead and no further spawns occur until the app restarts

#### Scenario: Shutdown
- **WHEN** the app shuts down after spawning its own lightpanda
- **THEN** the app gracefully stops the child (terminate, then kill if it does not exit in time), and an instance owned by someone else is untouched

### Requirement: Loopback bypass of the host-wide proxy
The host SHALL NOT route requests to the configured lightpanda URL through the host-wide HTTP proxy when that URL targets the loopback interface, regardless of `http_proxy`/ini proxy settings.

#### Scenario: Proxy configured for sites, loopback direct
- **WHEN** the ini proxy is set to an upstream proxy and a plugin calls `host.browser.call` with the default `http://127.0.0.1:8899`
- **THEN** the MCP HTTP requests go directly to loopback without touching the proxy

### Requirement: Image referer sentinel
The plugin manifest SHALL support `image_referer: "none"`, which instructs the host's image fetch path to send no `Referer` header (rather than deriving the plugin origin) on `/cache/img` upstream fetches. Any other value SHALL retain the current behavior (explicit Referer, or origin derived from the plugin website when omitted).

#### Scenario: Sentinel suppresses the header
- **WHEN** a plugin whose manifest declares `image_referer: "none"` serves reader images and `/cache/img` performs a miss
- **THEN** the upstream request carries no Referer header, and the CDN response is accepted like a direct fetch

#### Scenario: Other values unchanged
- **WHEN** a plugin declares no `image_referer` or a non-sentinel value
- **THEN** the Referer derivation stays byte-identical to today's behavior
