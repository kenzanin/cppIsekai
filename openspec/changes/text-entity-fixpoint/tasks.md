# Tasks: text-entity-fixpoint

## T1 — shared core (src/plugin/host_text.hpp)

- [ ] 1.1 Short-circuit `unescape` when the input has no `&` (common path stays allocation-free).
- [ ] 1.2 Wrap the existing lexbor parse+extract in a bounded fixpoint loop (max 4 passes, stop on equality).
- [ ] 1.3 Add the lenient numeric sweep with the existing `re2::RE2` and `utf8proc_encode`; guard cp 0, surrogates, >0x10FFFF; leave invalid references verbatim.
- [ ] 1.4 Add `clean(s)` = `trim(unescape(s))` and a doc comment stating it is the display-text helper.

## T2 — runtime exposure (three thin bindings, no new import names)

- [ ] 2.1 Lua: register `host.text.clean` in `src/plugin/lua/bindings/text.cpp`.
- [ ] 2.2 JS: register `host.text.clean` in `src/plugin/js/bindings/text.cpp`.
- [ ] 2.3 WASM: register/forward `text_clean` in `src/plugin/wasm/imports/text.cpp` (host import + guest-visible name unchanged in shape).

## T3 — tests (tests/test_htmltext.cpp)

- [ ] 3.1 Single `&#39;`, double `&amp;#39;`, triple `&amp;039;`, hex `&#x27;` all decode to the same apostrophe string.
- [ ] 3.2 Plain text and a URL containing a real `&` pass through `clean` unchanged.
- [ ] 3.3 A pathological deeply-nested `&amp;` string terminates at 4 passes and does not throw.

## T4 — proving migration

- [ ] 4.1 `plugins_dev/lua/mangabuddy/main.lua`: replace private `deep_unescape` + numeric sweep with `host.text.clean`.
- [ ] 4.2 `luac -p` the plugin, purge its `api_cache` rows, force refresh, and confirm the detail title has a real apostrophe.

## T5 — gate

- [ ] 5.1 `xmake -y` clean with the clang toolchain, zero new warnings.
- [ ] 5.2 `xmake test` full suite green; template goldens unchanged (no template output change expected).
- [ ] 5.3 Live detail probe for the mangabuddy title; no literal `&#` or `&amp;` remains.
