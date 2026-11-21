# Proposal: text-entity-fixpoint

## Why

Scraped titles still expose literal `&#39;`, `&amp;#39;` and `&amp;039;` in the UI.
Unicode support is fine (Lua 5.4 `utf8` + utf8proc host helpers); the defect is that
HTML entity decoding is left to each plugin. `host.text.unescape` currently performs
a single lexbor parse, so multiply-encoded sites (mangabuddy triple-encodes and drops
the `#` from numeric references) need a private fixpoint + numeric sweep in the plugin.
That is duplicated host work and keeps producing user-visible "numbers in the title".

## What

Strengthen the existing shared text core so one call is always enough:

1. `unescape` decodes to a fixed point (max 4 passes) using the **existing lexbor**
   parse-and-extract path — no new dependency, no hand-written entity table.
2. A lenient numeric-reference sweep for `#`-less `&NNN;` uses the **existing RE2**
   matcher and **existing utf8proc** encoder.
3. Add `host.text.clean(s)` = `trim(unescape(s))` to the shared core and expose it in
   all three runtime bindings (Lua/JS/WASM).
4. Migrate mangabuddy off its private `deep_unescape` to the host helper as a
   proving case; lock the contract with unit tests.

## Non-goals

- No new library and no new xrepo package.
- No host-side rewriting of plugin payloads in `call_cached` — decoding `&` in
  `id`/`slug`/`url`/`cover` would corrupt cache keys and query strings.
- No UI-side `innerHTML` decoding (XSS risk).
- No change to `utf8.*` availability or to the plugin host-helper surface beyond `clean`.
