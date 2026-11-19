---
type: Decision
title: "Sanitizers are an opt-in debug build (--sanitize), never the release daemon"
description: "xmake option --sanitize={address,address,undefined,undefined,thread} instruments every target, but only in debug/releasedbg: release warn-and-skips because that binary is the running daemon. First run found two real leaks and cleared the last debug-only warnings."
tags: [sanitizer, asan, ubsan, tsan, leaks, xmake, lexbor, quickjs]
generated: { by: agent/mcp, at: "2026-11-19T13:55:35Z" }
status: stable
---

## Decision

`xmake.lua` defines `option("sanitize")` (default `none`). Non-none adds
`-fsanitize=<v> -fno-omit-frame-pointer` to cxflags/cflags/ldflags for every target and
defines `CPPISEKAI_SANITIZE=<v>`.

```bash
xmake f -m debug --sanitize=address,undefined -y && xmake -y && xmake test
xmake f -m debug --sanitize=thread -y && xmake -y
xmake f -m release --sanitize=none -y && xmake -y     # back to the daemon build
```

In `release` the option prints `[sanitize] IGNORED in release mode` and applies nothing:
`build/linux/x86_64/release/cppIsekai` is the binary the daemon runs, and instrumenting it
in place would silently replace the running build (and cost 20-30x on the hot path).

Rationale: the C++ safety argument here is discipline + tooling, not the language. ASan/UBSan
is the cheapest way to catch what `-Wall -Wextra` and clang-analyzer cannot (lifetime, leaks,
integer UB) without changing language or dependencies.

## First run results (2026-11-19)

- `src/plugin/js/js_bindings.cpp` `arg_str`: `JS_ToCStringLen` copy never freed → leaked every
  string converted across the JS bridge. Fixed with `JS_FreeCString`.
- `src/plugin/htmltext_html.cpp` `select_each`: `lxb_css_selectors_parse` returns a selector
  list owning memory outside the CSS parser, so `parser_destroy` never freed it → ~20 KB leaked
  per `host.html.select/attr` call, i.e. every plugin page parse. Fixed with a
  `lxb_css_selector_list_destroy_memory` guard. ASan: 556 KB / 25 calls → 0.
- Suite is 19/19 green under `address,undefined` with leak detection on.
- `tests/test_stale.cpp` needed a link stub for `plugin::Host::cache_ttl_seconds` (the target
  links STORE_SRC only; every case passes `plugins == nullptr`). The break only appeared on a
  relink, so a stale release binary had been hiding it.

## Notes

- ASan leak reports are process exit-code 1 → `xmake test` reds. Read `Direct leak` rows first;
  `Indirect leak` rows are only meaningful after the direct ones are clean.
- Re-run the pass after touching any C-library boundary (lexbor, quickjs-ng, lua, wasm3).
- TSan cannot combine with ASan; it needs its own configure + full rebuild.

# Related Concepts
- Related to [xmake build system alongside cmake + prod-code gate tasks](xmake-build-system.md)
