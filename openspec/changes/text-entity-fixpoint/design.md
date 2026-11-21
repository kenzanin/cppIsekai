# Design: text-entity-fixpoint

## D1 — Reuse lexbor, do not write an entity table

`host_text::unescape` already parses the string with `lxb_html_document_parse` and
extracts text content via `lxb_dom_node_text_content`. That is the decoder. The fixpoint
loop simply calls the same library path again on the previous pass's output. No entity
map, no `&amp;` table, no new package.

## D2 — Bounded fixpoint

Decode up to 4 passes; stop when the output equals the input. 4 covers the observed
single (weebcentral), double, and triple (mangabuddy) encodings. The cap prevents a
pathological `&amp;amp;...` input from spinning forever. On lexbor parse failure the
current pass returns its input verbatim; the loop then terminates on the equality check.

## D3 — Lenient numeric sweep via RE2 + utf8proc

Sites sometimes emit `&amp;039;` — after lexbor decoding that becomes `&039;`, an
invalid numeric reference that lexbor will not touch. Sweep with the project's existing
`re2::RE2` (pattern `&(?:#)?([0-9]{1,7}|[xX][0-9a-fA-F]{1,6});`), parse the codepoint,
and encode it with `utf8proc_encode` (already included in `host_text.hpp`). Guard:
skip codepoint 0, surrogates (0xD800-0xDFFF) and values above 0x10FFFF; leave the
literal text unchanged on any invalid value. This uses libraries for both matching and
encoding — no hand-rolled UTF-8 writer.

Order inside one pass: lexbor decode → lenient numeric sweep → compare with pass input.

## D4 — `host.text.clean`

Shared core: `std::string clean(std::string_view s) { return trim(unescape(s)); }`.
Expose as `host.text.clean` in `src/plugin/lua/bindings/text.cpp`,
`src/plugin/js/bindings/text.cpp`, and `src/plugin/wasm/imports/text.cpp`. All three
bindings are thin marshalers over the one core, preserving porting parity (#5326).

## D5 — Rejected: payload-level normalisation in `call_cached`

A global deep-decode of plugin envelopes would also rewrite `&` inside `url`, `cover`,
`id` and `slug`, breaking signed query strings and cache keys. It also blurs the
host/plugin boundary (#5411: site adaptation stays in the plugin). Plugins must call
`clean` on the fields they know are display text.

## D6 — Tests

`tests/test_htmltext.cpp` gets cases for: single `&#39;`, double `&amp;#39;`, triple
`&amp;039;`, hex `&#x27;`, plain text unchanged, and a URL containing a real `&`
passed through `clean` unchanged. The URL case guards against over-eager decoding.

## D7 — Proving migration

`plugins_dev/lua/mangabuddy/main.lua` currently carries a private fixpoint + numeric
sweep. Replace it with `host.text.clean` and verify the detail title still contains a
real apostrophe after a forced refresh. This is the only plugin touched in this change;
a fleet-wide migration is a separate follow-up.

## Risks

- Over-decoding a title that literally contains `&amp;` as text. Accepted: such
  strings are overwhelmingly entity leakage from scraped HTML, and `clean` is opt-in per
  field, not applied globally.
- Lexbor parse cost: at most 4 parses per text field, only when the string contains
  `&`. Short-circuit when no `&` is present so the common path stays free.
