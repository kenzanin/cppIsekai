# Design Decisions — Host-Tool Gap Closure

## D1a — P1: host.plugin.param + host.http.fetch_body (20 lines, std lib)
- `host.plugin.param(key)` = read `main.json` `params` at plugin load; store in `sol::table`; return `string`
- `host.http.fetch_body(url, opts)` = `host.http.fetch(url, opts)` + return `res.body` if `res.status == 200`, else `nil`
- **Why**: 42 + 99 call sites; no new lib; porting becomes copy-paste
- **Files**: `src/plugin/lua/lua_runtime.cpp`, `src/plugin/js/js_bindings.cpp`, `src/plugin/wasm/wasm_imports.cpp`

## D1b — P2: host.text.normalize_status / chapter_num / date_to_iso (150 lines, std + RE2)
- `host.text.normalize_status(str)` = port `pluginutil/status.go` map verbatim: "ongoing"|"releasing"|"publishing" → "Ongoing"; "completed"|"finished" → "Completed"; "hiatus"|"on hiatus" → "Hiatus"; "dropped"|"cancelled" → "Dropped"; "upcoming"|"not yet released" → "Upcoming"; empty → "unknown"; else pass-through
- `host.text.chapter_num(str)` = port `extract.go` regexes to RE2: `chapterMarker` + `firstNumber` + comma-decimal handling
- `host.text.date_to_iso(str)` = port `dates.go` `dateLayouts` table (unix s/ms, RFC3339, "02 Jan 2006", "Jan 2, 2006") + `std::chrono` parse loop + relative phrase map ("3 days ago", "yesterday")
- **Why**: 75 call sites; pure host vocab, kills `helpers.lua` copy-paste
- **Files**: `src/plugin/host_text.hpp` (new, header-only)

## D1c — P3: host.text.trim / unescape / strip_html (100 lines, lexbor + utf8proc)
- `host.text.trim(str)` = utf8proc-aware trim (not ASCII-only)
- `host.text.unescape(str)` = lexbor entity tables (`lexbor_core_unicode_entities.c`)
- `host.text.strip_html(str)` = lexbor parse + walk text nodes + trim + unescape
- **Why**: 134 call sites across 6 `util.lua` files; kills `trim(unescape(x))` chains
- **Files**: `src/plugin/host_text.hpp` (new)

## D1d — P4: host.re2.find_all / gmatch / replace / quote / find_index (50 lines, RE2)
- `host.re2.find_all(pattern, text)` = `RE2::FindN` loop → array of array captures
- `host.re2.gmatch(pattern, text)` = sol2 closure over vector (pattern already in `lua_runtime.cpp:101` for `io.lines`)
- `host.re2.replace(pattern, repl, text)` = `RE2::ReplaceAll` + `RE2::GlobalReplace` (function repl)
- `host.re2.quote(str)` = manual escape (RE2 has no public quote)
- `host.re2.find_index(pattern, text)` = `RE2::Find` + position loop
- **Why**: 72+ call sites; biggest regex gap
- **Files**: `src/plugin/htmltext_re2.cpp` (new functions), `src/plugin/lua/lua_runtime.cpp` (bindings)

## D1e — P5: host.http.get_json / post_json (15 lines, glaze)
- `host.http.get_json(url, headers)` = `host.http.fetch` + `glz::read` + return table-or-nil
- `host.http.post_json(url, body, headers)` = same with method POST
- **Why**: 99 call sites; removes most-duplicated helper in corpus (fetch+decode+pcall+nil boilerplate)
- **Files**: `src/plugin/lua/lua_runtime.cpp`, `src/plugin/js/js_bindings.cpp`, `src/plugin/wasm/wasm_imports.cpp`

## D1f — P6: host.codecs.base64_encode / decode + hex_encode / decode (30 lines, std)
- `host.codecs.base64_encode(data)` = `std::format` + manual base64
- `host.codecs.base64_decode(str)` = manual base64 decode
- `host.codecs.hex_encode(data)` = `std::format("%02x")` loop
- `host.codecs.hex_decode(str)` = manual hex decode
- **Why**: 12 call sites; no new lib needed
- **Files**: `src/plugin/host_codecs.hpp` (new, header-only)

## D1g — P7: host.crypto.sha256_hex / md5_hex / hmac_sha256_hex / aes_gcm_decrypt (100 lines, OpenSSL)
- `host.crypto.sha256_hex(data)` = OpenSSL `EVP_Digest` + hex
- `host.crypto.md5_hex(data)` = OpenSSL `EVP_Digest` + hex
- `host.crypto.hmac_sha256_hex(data, key)` = OpenSSL `EVP_MAC` + hex
- `host.crypto.aes_gcm_decrypt(iv, ct, tag, key)` = OpenSSL `EVP_Decrypt` AES-GCM + hex
- **Why**: 5+3 call sites (comix, 1manga, mangahub); unblocks 3 sites that currently cannot be ported
- **Decision**: **OpenSSL** (system Arch) — already linked via `libcurl-impersonate-chrome.so` (BoringSSL), but need explicit `add_requires("openssl")` in `xmake.lua` for clean API access
- **Alternative rejected**: libsodium (xrepo) — more dep, no AES-GCM in libsodium's AEAD (only ChaCha20-Poly1305)
- **Files**: `src/plugin/host_crypto.hpp` (new, header-only), `xmake.lua` (add openssl dep)

## D1h — P8: host.browser.* — SKIPPED
- **Reason**: CDP cookie duration too short (session cookies expire in seconds); curl-impersonate already covers request fingerprint for token-signing sites
- **Decision**: Defer comix/mangataro/batcave porting until browser strategy resolved

## D1i — P9: host.html.xpath_* — IMPLEMENT (pugixml v1.16, xrepo)
- **Reason**: pugixml v1.16 di xrepo, light-weight XML parser with XPath support (zeux/pugixml)
- **Design**: `host.html.xpath(expr)` = lexbor parse HTML → serialize as XML → pugixml `select_nodes` → return text/attr nodes
- **Why**: XPath is a real gap (6 call sites); pugixml is small (~200KB), fast, and already in xmake-repo
- **Library**: pugixml v1.16 (xrepo)
- **Files**: `src/plugin/host_xpath.hpp` (new, header-only), `xmake.lua` (add pugixml dep)
