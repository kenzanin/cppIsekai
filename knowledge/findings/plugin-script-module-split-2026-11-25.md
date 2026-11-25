---
type: Finding
title: All Lua/JS plugin scripts split into per-action modules; helper duplication census
description: "Split 44 Lua/JS plugin entries into main wiring + common + per-action modules (Lua require / JS CommonJS), 5 batches ending 150e7ed. Behavior preserved: luac/node --check green, active plugins probed; mangadex SSL / upstream 403s reproduced on original files. Gotchas: JS global const TDZ across required modules; Lua generic-for vars const; existing genres/helpers modules need _action rename; stray/local functions between methods must be exported from common. Census: 195 helper defs / 115 names; find1 x20 (12 exact), get_json x9, http_req x8, clean x8, epoch x7, normalize x2, abs_url x2 -- all one-line delegates to existing host helpers (re2.capture, http.get_json/fetch_body, text.clean/date_epoch/normalize_status, url_abs), so no new host primitive is justified. Real dup: batcave+mangabay DLE wrappers and enrichment clean_text/normalize -- shared plugins_lib candidates."
tags: [plugins, refactor, module-split, duplication, host-helpers]
generated: { by: agent/mcp, at: "2026-11-25T06:27:46Z" }
status: stable
---

# Related Concepts
- Related to [Source layout: crown facades, ~200-loc units, clang-doc comments](../decisions/source-layout-policy.md)
