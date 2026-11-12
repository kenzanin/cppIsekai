# Spec Delta

## MODIFIED Requirements

### Requirement: Time access
Plugins SHALL call `now()` to get the current time as INTEGER unix seconds, matching the host-wide unix-time contract.

#### Scenario: Plugin stamps a fetch
- **WHEN** a plugin calls `now()` and includes the value in its result
- **THEN** the value is unix seconds, consistent with host-generated timestamps

### Requirement: Host-tool parity across runtimes
Every host tool on the Lua surface SHALL be implementable by the JS and WASM bindings with the same request/response semantics, or the gap SHALL be explicitly documented as out of scope in the runtime's contract. The parity set is:

| Tool | Lua | JS | WASM |
|---|---|---|---|
| `now` | yes | yes | yes |
| `log.info` / `log.error` | yes | yes | yes (`log_info`; error maps to the same import with a level byte) |
| `http.fetch` | yes | yes | yes |
| `storage.get` / `storage.set` | yes | yes | yes |
| `fs.read` | yes | yes | yes |
| `config.get` | yes | yes (this change) | out of scope |
| `url_encode` / `url_decode` | yes | yes (this change) | yes (this change) |
| `crypto.vrf_sign` | yes | yes (this change) | out of scope |
| `re2.match` / `re2.find` | yes | yes | yes |
| `html.select` / `html.attr` | yes | yes | yes |

`html.select`/`html.attr` double as the list selectors: they return arrays of all matched elements (trimmed text for `select`, attribute string for `attr`) in all three runtimes — there is no separate `find_list_*` tool and none is added. Ports migrating goIsekai plugins SHALL map `host.html.find_list_text(html, css)` → `host.html.select(html, css)` and `host.html.find_list_attr(html, css[, attr])` → `host.html.attr(html, css, attr)` (default attr `text` where applicable).

#### Scenario: url_encode round-trips in JS and WASM
- **WHEN** a JS or WASM plugin encodes a string with reserved characters (`a b&c=d%?`)
- **THEN** the encoded form percent-encodes exactly the characters that `url_decode` accepts to return the original string

#### Scenario: config.get in JS matches Lua behavior
- **WHEN** a JS plugin calls `config.get("client_id")` with a `config.json` containing `client_id` and then for an absent key
- **THEN** it receives the stored value for the present key and nil for the absent key

#### Scenario: vrf_sign in JS matches the Lua vectors
- **WHEN** a JS plugin calls `crypto.vrf_sign("/titles", {limit="50"}, VRF_STAGES)` with the same fixed table set used in `tests/test_vrf.cpp`
- **THEN** the result equals the base64url signature the Lua binding produces for the same arguments

#### Scenario: http.fetch opts behave the same in all three runtimes
- **WHEN** a JS or WASM plugin passes `{timeout_ms, profile, method, body, headers}`
- **THEN** the request is issued with those values exactly as the Lua path would, and the reply shape is `{status, body, headers}`
