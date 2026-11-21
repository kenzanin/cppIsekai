# Tasks: text-entity-fixpoint

## T1 — shared core (src/plugin/host_text.hpp)

- [x] 1.1 Short-circuit `unescape` when the input has no `&` (common path stays allocation-free).
- [x] 1.2 Wrap the existing lexbor parse+extract in a bounded fixpoint loop (max 4 passes, stop on equality).
- [x] 1.3 Add the lenient numeric sweep with the existing `re2::RE2` and `utf8proc_encode`; guard cp 0, surrogates, >0x10FFFF; leave invalid references verbatim.
- [x] 1.4 Add `clean(s)` = `trim(unescape(s))` and a doc comment stating it is the display-text helper.

## T2 — runtime exposure (three thin bindings, no new import names)

- [x] 2.1 Lua: register `host.text.clean` in `src/plugin/lua/bindings/rest.cpp` (the real home of the `host.text` table; there is no lua `bindings/text.cpp` and xmake.lua has an explicit file list).
- [x] 2.2 JS: register `host.text.clean` in `src/plugin/js/bindings/text.cpp`.
- [x] 2.3 WASM: register/forward `text_clean` in `src/plugin/wasm/imports/text.cpp` (host import + guest-visible name unchanged in shape).

## T3 — tests (tests/test_htmltext.cpp)

- [x] 3.1 Single `&#39;`, double `&amp;#39;`, triple `&amp;039;`, hex `&#x27;` all decode to the same apostrophe string.
- [x] 3.2 Plain text and a URL containing a real `&` pass through `clean` unchanged.
- [x] 3.3 A pathological deeply-nested `&amp;` string terminates at 4 passes and does not throw.

## T4 — proving migration

- [x] 4.1 `plugins_dev/lua/mangabuddy/main.lua`: replace private `deep_unescape` + numeric sweep with `host.text.clean`.
- [x] 4.2 `luac -p` the plugin (done), purge its `api_cache` rows, force refresh, and confirm the detail title has a real apostrophe. (live probe: 24 cache rows purged, daemon restarted, `/plugin/mangabuddy/detail` + `/api/detail` both return `A Middle-Aged Man's Isekai Gender Reincarnation`)

## T5 — gate

- [x] 5.1 `xmake -y` clean with the clang toolchain, zero new warnings.
- [x] 5.2 `xmake test` full suite green; template goldens unchanged (no template output change expected).
- [x] 5.3 Live detail probe for the mangabuddy title; no literal `&#` or `&amp;` remains. (verified: both absent from the full detail envelope)
