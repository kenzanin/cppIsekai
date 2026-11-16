---
type: Constraint
title: host.text.trim / strip_html preserve casing
description: "host.text.trim strips only leading/trailing whitespace and control codepoints; it must not casefold, and strip_html keeps text casing. Plugins must not rely on trim to normalize status strings."
tags: [host-tools, text, plugin, casefold, trim]
generated: { by: agent/mcp, at: "2026-11-16T10:53:26Z" }
status: stable
---

`plugin::host_text::trim` previously ran utf8proc NFKD + casefold, so every plugin title that went through the Lua/JS/WASM `host.text.trim` lost its casing (`Martial Peak` -> `martial peak`).

Now it strips leading/trailing whitespace and control codepoints only (space, tab, CR/LF/FF, NBSP, U+2000-U+200F, U+2028/2029, U+3000, U+FEFF, < U+0020) and leaves the remaining bytes untouched; `strip_html` likewise preserves casing.

Status canonicalization stays in `host.text.normalize_status`; titles/metadata keep their original casing. Regression checks live in tests/test_plugin.cpp (`trim("  Martial Peak \n") == "Martial Peak"`).

# Related Concepts
- Related to [Plugin system: folder packaging, manifest, multi-runtime data-contract ABI](../architecture/plugin-system.md)
