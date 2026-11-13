# Spec Delta — Host-Tool Gap Closure

## ADDED Requirements

### REQ-HOST-TEXT: host.text.* — Text utilities
- `host.text.normalize_status(str)` = normalize status vocabulary ("ongoing"→"Ongoing", "completed"→"Completed", "hiatus"→"Hiatus", "dropped"→"Dropped", "upcoming"→"Upcoming")
- `host.text.chapter_num(str)` = extract chapter number as float ("Ch. 12.5" → 12.5)
- `host.text.date_to_iso(str)` = parse human date to ISO 8601 ("3 days ago" → "2026-11-10", "02 Jan 2006" → "2006-01-02")
- `host.text.trim(str)` = UTF-8-aware trim (utf8proc)
- `host.text.unescape(str)` = HTML entity decode (lexbor)
- `host.text.strip_html(str)` = strip HTML tags, trim, unescape (lexbor)
- **Applies to**: all three runtimes (Lua, JS, WASM)
- **Library**: lexbor + utf8proc (already in xrepo)
- **Files**: `src/plugin/host_text.hpp`

### REQ-HOST-RE2: host.re2.* — Regex utilities
- `host.re2.find_all(pattern, text)` = all matches with captures (array of array)
- `host.re2.gmatch(pattern, text)` = Lua iterator over matches
- `host.re2.replace(pattern, repl, text)` = replace all with string or function
- `host.re2.quote(str)` = escape regex metacharacters
- `host.re2.find_index(pattern, text)` = byte positions of matches
- **Applies to**: all three runtimes (Lua, JS, WASM)
- **Library**: RE2 (already in xrepo)
- **Files**: `src/plugin/htmltext_re2.cpp`

### REQ-HOST-HTTP-SUGAR: host.http.* — HTTP sugar
- `host.http.get_json(url, headers)` = GET + JSON decode → table-or-nil
- `host.http.post_json(url, body, headers)` = POST + JSON decode → table-or-nil
- `host.http.fetch_body(url, opts)` = fetch + body-or-nil (200 only)
- **Applies to**: all three runtimes (Lua, JS, WASM)
- **Library**: glaze (already in xrepo)
- **Files**: `src/plugin/lua/lua_runtime.cpp`, `src/plugin/js/js_bindings.cpp`, `src/plugin/wasm/wasm_imports.cpp`

### REQ-HOST-CODECS: host.codecs.* — Encoding utilities
- `host.codecs.base64_encode(data)` = base64 encode
- `host.codecs.base64_decode(str)` = base64 decode
- `host.codecs.hex_encode(data)` = hex encode
- `host.codecs.hex_decode(str)` = hex decode
- **Applies to**: all three runtimes (Lua, JS, WASM)
- **Library**: std (no new dep)
- **Files**: `src/plugin/host_codecs.hpp`

### REQ-HOST-CRYPTO: host.crypto.* — Cryptographic utilities
- `host.crypto.sha256_hex(data)` = SHA-256 hash as hex
- `host.crypto.md5_hex(data)` = MD5 hash as hex
- `host.crypto.hmac_sha256_hex(data, key)` = HMAC-SHA256 as hex
- `host.crypto.aes_gcm_decrypt(iv, ct, tag, key)` = AES-256-GCM decrypt → plaintext or nil
- **Applies to**: all three runtimes (Lua, JS, WASM)
- **Library**: OpenSSL (system Arch)
- **Files**: `src/plugin/host_crypto.hpp`

### REQ-HOST-PLUGIN: host.plugin.* — Plugin metadata
- `host.plugin.param(key)` = read `main.json` `params[key]`
- **Applies to**: all three runtimes (Lua, JS, WASM)
- **Library**: std
- **Files**: `src/plugin/lua/lua_runtime.cpp`, `src/plugin/js/js_bindings.cpp`, `src/plugin/wasm/wasm_imports.cpp`

## REMOVED Requirements

### REQ-HOST-BROWSER: host.browser.* — Browser CDP (REMOVED)
- **Reason**: CDP cookie duration too short (session cookies expire in seconds); curl-impersonate already covers request fingerprint
- **Decision**: Defer comix/mangataro/batcave porting until browser strategy resolved

### REQ-HOST-XPATH: host.html.xpath — XPath (IMPLEMENTED)
- `host.html.xpath(expr, html)` = parse HTML, evaluate XPath, return text/attr nodes
- **Applies to**: all three runtimes (Lua, JS, WASM)
- **Library**: pugixml v1.16 (xrepo)
- **Files**: `src/plugin/host_xpath.hpp`
