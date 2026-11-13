# Spec Delta: host-tools

## ADDED Requirements

### Requirement: host.text utilities
The host SHALL expose `host.text.*` in Lua, JS, and WASM: `normalize_status(str)` mapping
status vocabulary to canonical values (ongoing→Ongoing, completed→Completed,
hiatus→Hiatus, dropped→Dropped, upcoming→Upcoming); `chapter_num(str)` returning the
chapter number as a float ("Ch. 12.5" → 12.5); `date_to_iso(str)` parsing absolute and
relative human dates to ISO 8601 (`YYYY-MM-DD`); `trim(str)` UTF-8-aware; `unescape(str)`
HTML entity decode; `strip_html(str)` removing tags then trimming and unescaping. The
shared implementation SHALL live in `src/plugin/host_text.hpp` (utf8proc + lexbor +
date v3.0.4); runtime bindings SHALL be thin marshals over that core.

#### Scenario: normalize status vocabulary
- **WHEN** a plugin calls `host.text.normalize_status("releasing")`
- **THEN** the host returns `"Ongoing"` (synonym map)

#### Scenario: chapter number extraction
- **WHEN** a plugin calls `host.text.chapter_num("Ch. 12.5")`
- **THEN** the host returns `12.5`

#### Scenario: relative date parsing
- **WHEN** a plugin calls `host.text.date_to_iso("3 days ago")`
- **THEN** the host returns the ISO date three days before today as `YYYY-MM-DD`

#### Scenario: strip HTML from a fragment
- **WHEN** a plugin calls `host.text.strip_html("<p>Hello <b>world</b></p>")`
- **THEN** the host returns `"Hello world"`

### Requirement: host.re2 utilities
The host SHALL expose `host.re2.*` in Lua, JS, and WASM: `find_all(pattern, text)`
returning every match with captures, `gmatch(pattern, text)`, `replace(pattern, repl,
text)`, `quote(str)`, and `find_index(pattern, text)`. `replace` SHALL accept
goIsekai's Go `$N`/`${N}` backreference syntax by translating it to RE2's `\N` inside
the shared core `plugin::re2_replace`, so all three runtimes inherit one behavior.
Invalid patterns SHALL return an error/empty result rather than crash.

#### Scenario: find_all returns every match
- **WHEN** a plugin calls `host.re2.find_all("\\d+", "abc 123456")`
- **THEN** the result contains both matches `"123"` and `"456"` (each with its capture groups), not duplicates

#### Scenario: goIsekai backreference syntax in replace
- **WHEN** a plugin calls `host.re2.replace("(\\w+)@\\w+", "$1", "bob@example")`
- **THEN** the result is `"bob"` — `$1` is accepted via the shared-core translation

### Requirement: host.http sugar
The host SHALL expose `host.http.fetch_body(url, opts)` returning the response body
only on HTTP 200 and nil otherwise, plus `host.http.get_json(url, headers)` and
`host.http.post_json(url, body, headers)` performing fetch + JSON decode to a
table/object, returning nil/null on transport failure, non-200 status, or malformed
JSON. WASM (no table type) SHALL return the body as JSON text only when it parses as
JSON, else `kNotFound`, matching the text-in/text-out import convention.

#### Scenario: get_json decodes a JSON body
- **WHEN** a plugin calls `host.http.get_json(url)` and the endpoint returns 200 with a JSON object
- **THEN** the host returns the decoded table/object

#### Scenario: get_json failure returns nil
- **WHEN** the request fails, returns non-200, or the body is not valid JSON
- **THEN** the host returns nil/null with no partial data

#### Scenario: fetch_body gates on 200
- **WHEN** a plugin calls `host.http.fetch_body(url)` and the endpoint returns 404
- **THEN** the host returns nil

### Requirement: host.codecs
The host SHALL expose `host.codecs.base64_encode(str)`, `base64_decode(str)`,
`hex_encode(str)`, and `hex_decode(str)` in all three runtimes, with decode
returning nil/null on invalid input. The shared implementation SHALL live in
`src/plugin/host_codecs.hpp`; standard base64 decode SHALL reuse the existing strict
`vrf_b64_decode` rather than a second decoder table.

#### Scenario: base64 known vector
- **WHEN** a plugin calls `host.codecs.base64_encode("hello")`
- **THEN** the host returns `"aGVsbG8="`

#### Scenario: invalid base64 rejected
- **WHEN** a plugin calls `host.codecs.base64_decode` with invalid characters or an
  impossible quantum (`%4==1`)
- **THEN** the host returns nil/null

### Requirement: host.crypto hashes and AES-GCM decrypt
The host SHALL expose `host.crypto.sha256_hex(data)`, `host.crypto.md5_hex(data)`,
`host.crypto.hmac_sha256_hex(data, key)` backed by xrepo `hash-library`, and
`host.crypto.aes_gcm_decrypt(iv, ct, tag, key)` performing AES-256-GCM decryption
backed by the local `tiny-aes256` target with CTR keystream + GHASH implemented in
`src/plugin/host_crypto.hpp`. All functions take hex input and SHALL return
nil/null on invalid hex, wrong key length, or authentication failure; the GCM tag
SHALL be verified before any plaintext is returned (no unverified output).

#### Scenario: SHA-256 known-answer vector
- **WHEN** a plugin calls `host.crypto.sha256_hex("abc")`
- **THEN** the host returns `ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad`

#### Scenario: tampered GCM tag rejected
- **WHEN** a plugin calls `host.crypto.aes_gcm_decrypt` with a corrupted tag
- **THEN** the host returns nil and no plaintext is emitted

### Requirement: host.plugin.param
The host SHALL expose `host.plugin.param(key)` in Lua, JS, and WASM, returning the
value of `key` from the plugin's own `main.json` `params` object (parsed at load
time), and nil/null when the key is absent.

#### Scenario: param lookup
- **WHEN** a plugin whose `main.json` has `params: {"site_url": "https://x"}` calls `host.plugin.param("site_url")`
- **THEN** the host returns `"https://x"`

### Requirement: host.html.xpath
The host SHALL expose `host.html.xpath(expr, html)` in Lua, JS, and WASM: parse the
HTML with lexbor, serialize with void-element and entity fixup, evaluate the XPath
with pugixml (`select_nodes`), and return matching text/attribute values as an array
in document order. Parse failure, invalid expression, or non-node-set results
(e.g. `count(...)`) SHALL return nil/null. The shared implementation SHALL live in
`src/plugin/host_xpath.hpp` (pugixml v1.16 from xrepo).

#### Scenario: attribute selection
- **WHEN** a plugin calls `host.html.xpath("//div/@class", "<div class=\"a\">x</div>")`
- **THEN** the result contains `"a"`

#### Scenario: invalid expression returns nil
- **WHEN** the XPath is malformed or returns a scalar instead of a node-set
- **THEN** the host returns nil/null

#### Scenario: void element roundtrip
- **WHEN** a plugin evaluates an XPath whose match crosses a `<br>` void element
- **THEN** the document parses (void self-closing fixup applied) and the matched text
  values are returned
