# Tasks — Host-Tool Gap Closure

## Phase 1: P1 — host.plugin.param + host.http.fetch_body (20 lines, std lib)

- [ ] 1.1 **P1**: `host.plugin.param(key)` = read `main.json` `params` at load; store in `sol::table`; return `string`
  - [ ] Lua binding in `src/plugin/lua/lua_runtime.cpp`
  - [ ] JS binding in `src/plugin/js/js_bindings.cpp`
  - [ ] WASM binding in `src/plugin/wasm/wasm_imports.cpp`
- [ ] 1.2 **P1**: `host.http.fetch_body(url, opts)` = `host.http.fetch` + return `res.body` if `res.status == 200`, else `nil`
  - [ ] Lua binding in `src/plugin/lua/lua_runtime.cpp`
  - [ ] JS binding in `src/plugin/js/js_bindings.cpp`
  - [ ] WASM binding in `src/plugin/wasm/wasm_imports.cpp`
- [ ] 1.3 Build + test (17/17)
- [ ] 1.4 Commit: `plugin: host.plugin.param + host.http.fetch_body (P1, 42+99 sites)`

## Phase 2: P2 — host.text.normalize_status / chapter_num / date_to_iso (150 lines, std + RE2)

- [ ] 2.1 **P2**: `host.text.normalize_status(str)` = port `pluginutil/status.go` map verbatim
  - [ ] Header-only `src/plugin/host_text.hpp`
- [ ] 2.2 **P2**: `host.text.chapter_num(str)` = port `extract.go` regexes to RE2
  - [ ] Header-only `src/plugin/host_text.hpp`
- [ ] 2.3 **P2**: `host.text.date_to_iso(str)` = port `dates.go` `dateLayouts` + `std::chrono` parse loop + relative phrase map
  - [ ] Header-only `src/plugin/host_text.hpp`
- [ ] 2.4 Lua + JS + WASM bindings in `src/plugin/lua/lua_runtime.cpp`, `src/plugin/js/js_bindings.cpp`, `src/plugin/wasm/wasm_imports.cpp`
- [ ] 2.5 Build + test (17/17)
- [ ] 2.6 Commit: `plugin: host.text.normalize_status/chapter_num/date_to_iso (P2, 75 sites)`

## Phase 3: P3 — host.text.trim / unescape / strip_html (100 lines, lexbor + utf8proc)

- [ ] 3.1 **P3**: `host.text.trim(str)` = utf8proc-aware trim
  - [ ] Header-only `src/plugin/host_text.hpp` (extend)
- [ ] 3.2 **P3**: `host.text.unescape(str)` = lexbor entity tables
  - [ ] Header-only `src/plugin/host_text.hpp` (extend)
- [ ] 3.3 **P3**: `host.text.strip_html(str)` = lexbor parse + walk text nodes + trim + unescape
  - [ ] Header-only `src/plugin/host_text.hpp` (extend)
- [ ] 3.4 Lua + JS + WASM bindings
- [ ] 3.5 Build + test (17/17)
- [ ] 3.6 Commit: `plugin: host.text.trim/unescape/strip_html (P3, 134 sites)`

## Phase 4: P4 — host.re2.find_all / gmatch / replace / quote / find_index (50 lines, RE2)

- [ ] 4.1 **P4**: `host.re2.find_all(pattern, text)` = `RE2::FindN` loop → array of array captures
  - [ ] `src/plugin/htmltext_re2.cpp` (new function)
  - [ ] Lua binding in `src/plugin/lua/lua_runtime.cpp`
- [ ] 4.2 **P4**: `host.re2.gmatch(pattern, text)` = sol2 closure over vector (pattern already in `lua_runtime.cpp:101`)
  - [ ] Lua binding in `src/plugin/lua/lua_runtime.cpp`
- [ ] 4.3 **P4**: `host.re2.replace(pattern, repl, text)` = `RE2::ReplaceAll` + `RE2::GlobalReplace` (function repl)
  - [ ] `src/plugin/htmltext_re2.cpp`
  - [ ] Lua binding
- [ ] 4.4 **P4**: `host.re2.quote(str)` = manual escape (RE2 has no public quote)
  - [ ] `src/plugin/htmltext_re2.cpp`
  - [ ] Lua binding
- [ ] 4.5 **P4**: `host.re2.find_index(pattern, text)` = `RE2::Find` + position loop
  - [ ] `src/plugin/htmltext_re2.cpp`
  - [ ] Lua binding
- [ ] 4.6 JS + WASM bindings for all P4
- [ ] 4.7 Build + test (17/17)
- [ ] 4.8 Commit: `plugin: host.re2.find_all/gmatch/replace/quote/find_index (P4, 72+ sites)`

## Phase 5: P5 — host.http.get_json / post_json (15 lines, glaze)

- [ ] 5.1 **P5**: `host.http.get_json(url, headers)` = `host.http.fetch` + `glz::read` + return table-or-nil
  - [ ] Lua binding in `src/plugin/lua/lua_runtime.cpp`
  - [ ] JS binding in `src/plugin/js/js_bindings.cpp`
  - [ ] WASM binding in `src/plugin/wasm/wasm_imports.cpp`
- [ ] 5.2 **P5**: `host.http.post_json(url, body, headers)` = same with method POST
  - [ ] Lua + JS + WASM bindings
- [ ] 5.3 Build + test (17/17)
- [ ] 5.4 Commit: `plugin: host.http.get_json/post_json (P5, 99 sites)`

## Phase 6: P6 — host.codecs.base64_encode / decode + hex_encode / decode (30 lines, std)

- [ ] 6.1 **P6**: `host.codecs.base64_encode(data)` + `host.codecs.base64_decode(str)` = manual base64
  - [ ] Header-only `src/plugin/host_codecs.hpp` (new)
- [ ] 6.2 **P6**: `host.codecs.hex_encode(data)` + `host.codecs.hex_decode(str)` = manual hex
  - [ ] Header-only `src/plugin/host_codecs.hpp` (extend)
- [ ] 6.3 Lua + JS + WASM bindings
- [ ] 6.4 Build + test (17/17)
- [ ] 6.5 Commit: `plugin: host.codecs.base64/hex encode/decode (P6, 12 sites)`

## Phase 7: P7 — host.crypto.sha256_hex / md5_hex / hmac_sha256_hex / aes_gcm_decrypt (100 lines, OpenSSL)

- [ ] 7.1 Add OpenSSL dep in `xmake.lua`: `add_requires("openssl")` (system Arch already has it)
- [ ] 7.2 **P7**: `host.crypto.sha256_hex(data)` + `host.crypto.md5_hex(data)` = OpenSSL `EVP_Digest` + hex
  - [ ] Header-only `src/plugin/host_crypto.hpp` (new)
- [ ] 7.3 **P7**: `host.crypto.hmac_sha256_hex(data, key)` = OpenSSL `EVP_MAC` + hex
  - [ ] Header-only `src/plugin/host_crypto.hpp` (extend)
- [ ] 7.4 **P7**: `host.crypto.aes_gcm_decrypt(iv, ct, tag, key)` = OpenSSL `EVP_Decrypt` AES-GCM + hex
  - [ ] Header-only `src/plugin/host_crypto.hpp` (extend)
- [ ] 7.5 Lua + JS + WASM bindings
- [ ] 7.6 Build + test (17/17)
- [ ] 7.7 Commit: `plugin: host.crypto.sha256/md5/hmac/aes_gcm (P7, 5+3 sites, OpenSSL dep)`

## Phase 8: P8 — SKIPPED + P9 — IMPLEMENT (pugixml)

- [ ] 8.1 **P8**: `host.browser.*` — SKIPPED (CDP cookie duration too short)
- [ ] 8.2 **P9**: Add pugixml dep in `xmake.lua`: `add_requires("pugixml")` (v1.16, xrepo)
- [ ] 8.3 **P9**: `host.html.xpath(expr)` = lexbor parse HTML → serialize as XML → pugixml `select_nodes` → return text/attr nodes
  - [ ] Header-only `src/plugin/host_xpath.hpp` (new)
- [ ] 8.4 Lua + JS + WASM bindings
- [ ] 8.5 Build + test (17/17)
- [ ] 8.6 Commit: `plugin: host.html.xpath (P9, pugixml v1.16, 6 sites)`

## Phase 9: Documentation

- [ ] 9.1 Update `knowledge/references/host-tool-gap-analysis.md` to reflect implemented P1-P7
- [ ] 9.2 Update `openspec/changes/plugin-port-multi-runtime/tasks.md` to mark Phase 2/3 as complete
