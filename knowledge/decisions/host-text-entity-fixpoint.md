---
type: Decision
title: "host.text.clean: lexbor entity fixpoint + RE2/utf8proc numeric sweep"
description: "Display-text cleanup moved into the shared host core: unescape decodes lexbor entities to a fixed point (max 4 passes, skipped when the input has no '&'), a RE2+utf8proc sweep handles lenient numeric refs, and host.text.clean = trim(unescape) is exposed in lua/js/wasm."
tags: [host-tools, text, html-entities, lexbor, re2, utf8proc, plugin-abi, mangabuddy]
generated: { by: agent/mcp, at: "2026-11-21T05:30:46Z" }
status: stable
---

# host.text.clean: lexbor entity fixpoint + RE2/utf8proc numeric sweep

Change `text-entity-fixpoint` (OpenSpec), implemented 2026-11-21. Shared core is
`src/plugin/host_text.hpp`; the three runtime files are thin marshals.

## D1 — Reuse lexbor, no entity table
`host_text::unescape` already parses with `lxb_html_document_parse` and extracts
text via `lxb_dom_node_text_content`. The fixpoint reuses that exact path; no
hand-written entity map and no new dependency.

## D2 — Bounded fixpoint, free common path
Max 4 passes, stop on pass-input equality. The loop is skipped entirely (and no
HTML parse happens) when the input contains no `&`. 4 passes cover the observed
single/double/triple encodings and make pathological nesting terminate.

## D3 — Lenient numeric sweep: RE2 match + utf8proc encode
Sites drop the `#` (`&amp;039;` -> `&039;`), which lexbor leaves alone. Sweep with
the existing `re2::RE2` pattern `&(?:#)?([0-9]{1,7}|[xX][0-9a-fA-F]{1,6});` and
encode the codepoint with `utf8proc_encode_char`. Invalid codepoints (0,
0xD800-0xDFFF, > 0x10FFFF) stay literal. Because lexbor substitutes U+FFFD for
invalid numeric refs, their `&` is escaped to `&amp;` before the lexbor pass so
the reference survives verbatim; the next pass is then a no-op.

## D4 — host.text.clean
`clean(s) = trim(unescape(s))`, documented as the display-text helper. Exposed as
`host.text.clean` in lua (`src/plugin/lua/bindings/rest.cpp`, the real home of the
`host.text` table), js (`src/plugin/js/bindings/text.cpp` + registration in
`js_bindings.cpp`) and wasm (`text_clean` import in
`src/plugin/wasm/imports/text.cpp` + `wasm_imports.cpp`). The lua change spec
named a non-existent `lua/bindings/text.cpp`; xmake.lua has an explicit file list
and was not touched, so the table lives where it already was.

## D5 — Rejected: payload normalisation in call_cached
Deep-decoding whole plugin envelopes would rewrite `&` in `url`, `cover`, `id`
and `slug`, corrupting signed query strings and cache keys. Site adaptation
stays in the plugin; plugins call `clean` only on fields they know are display
text.

## D6 — Tests
`tests/test_htmltext.cpp` covers single/double/triple/hex and `&x27;`, plain text
and `https://x/api?a=1&b=2` passthrough, invalid refs (0, surrogate, > U+10FFFF)
staying literal, and 8-layer nesting terminating without throwing.

## D7 — Proving migration
`plugins_dev/lua/mangabuddy/main.lua` dropped its private `deep_unescape` +
numeric sweep for `host.text.clean` (title and description). `luac -p` passes.

## Gate
`xmake -y` clang build: 0 warnings. `xmake test`: 20/20 targets pass. Diff: 11
files, +180/-28. Live mangabuddy probe not run (no daemon restart in session).

# Related Concepts
- [Plugin host promotion: shared host cores, thin runtime marshals](plugin-host-promotion.md): Extends the plugin-host-promotion series: entity decoding/display cleanup is one more shared host core instead of per-plugin code (mangabuddy's private deep_unescape was the last copy).
