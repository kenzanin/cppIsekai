# plugin-helper Delta

## MODIFIED Requirements

### Requirement: Host tools are namespaced and identical across runtimes
All host-provided functions live under a single `host.*` namespace in every plugin
runtime, with identical names, argument order, and return shapes. The host-tool set is:
`host.now`, `host.log.info`, `host.log.error`, `host.http.fetch`, `host.storage.get`,
`host.storage.set`, `host.fs.read`, `host.re2.match`, `host.re2.find`,
`host.html.select`, `host.html.attr`. WASM exposes the same set as flat `env.*`
imports (`env.host_<group>_<fn>`) because wasm imports cannot nest.

#### Scenario: Parity across lua, js, and wasm
- **WHEN** a fixture plugin in each runtime (lua, js, wasm) inspects its host-tool
  surface and calls `host.now`, `host.http.fetch` against a local fixture server, and
  `host.storage.set`/`get` round-trip
- **THEN** every runtime resolves the full 11-tool set
- **AND** the returned JSON shapes are identical across the three runtimes

#### Scenario: Lua gains fs.read parity
- **WHEN** a lua fixture calls `host.fs.read("config.json")` from its plugin directory
- **THEN** the file body is returned under the same path confinement rules js/wasm use
- **AND** paths escaping the plugin directory are rejected identically to js/wasm

### Requirement: Regex host helper (RE2)
`host.re2.match(pattern, text)` returns an array of capture groups (element 0 = whole
match, then submatches; empty array when the pattern does not match).
`host.re2.find(pattern, text)` returns the first match as the same array form, or an
empty array when there is no match. Patterns are evaluated with linear-time RE2.

#### Scenario: Match and find on fixture text
- **WHEN** a plugin calls `host.re2.match("ch(\\d+)", "Chapter ch1050")` and
  `host.re2.find("(\\w+)@(\\w+)", "user@host other")`
- **THEN** match returns `["ch1050", "1050"]`
- **AND** find returns `["user@host", "user", "host"]`

#### Scenario: No match is not an error
- **WHEN** the pattern does not appear in the text
- **THEN** the result is an empty array and no error is raised

#### Scenario: Invalid pattern is loud
- **WHEN** the pattern fails RE2 compilation
- **THEN** the calling script receives a runtime error (lua error / JS exception /
  wasm trap) naming the compile failure

### Requirement: DOM host helpers (lexbor)
`host.html.select(html, css)` returns the trimmed text contents of all elements
matching the CSS selector. `host.html.attr(html, css, attr)` returns the values of the
named attribute for all matching elements that have it (elements without the attribute
are skipped). No-match yields an empty array; malformed HTML is parsed leniently.

#### Scenario: Select text from fixture HTML
- **WHEN** a plugin calls `host.html.select(fixture_html, "div.title > a")`
- **THEN** the array holds the trimmed text of each matched anchor in document order

#### Scenario: Attribute extraction skips missing
- **WHEN** matching elements include ones without the requested attribute
- **THEN** only elements that have the attribute contribute a value, in document order

#### Scenario: Empty results for no match and bad HTML
- **WHEN** the selector matches nothing or the HTML is truncated/malformed
- **THEN** the helpers return an empty array and do not throw
