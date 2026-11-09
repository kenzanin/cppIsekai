---
type: decision
title: "Plugin runtimes: wasm3 + quickjs-ng execution contract"
description: "plugin-helper landed: unified host.* namespace (11 tools) across lua/js/wasm, RE2 + lexbor heavy helpers behind one htmltext core, lua fs.read parity closed; lexbor vendor pitfalls (ports/posix memory.c, separate selectors/ subtree) recorded."
tags: [plugin-runtime, host-tools, re2, lexbor, parity, helper-api, html, regex]
generated: { by: agent/mcp, at: "2026-11-09T07:35:16Z" }
status: stable
---

DECISION (change plugin-helper, merged cd87738, archived 2026-11-09; 6/6 tasks, ctest 11/11, live smoke ok):

HOST NAMESPACE. All host tools live under one `host.*` surface in lua and js; bare globals (http/storage/log/now/fs) removed, no aliases. WASM exposes flat env.* imports (cannot nest): now, log_info, fs_read, storage_get/set, http_fetch, re2_match/find, html_select/attr. 11-tool set: host.now, host.log.info/error, host.http.fetch, host.storage.get/set, host.fs.read, host.re2.match/find, host.html.select/attr.

HELPERS (one core src/plugin/htmltext.{hpp,cpp}, three thin marshals):
- re2 (system lib, linear-time/no ReDoS): match anchors both ends, find first-found; groups vector (element 0 whole match); no-match = empty (never error); bad pattern throws invalid_argument (lua->runtime_error, js->TypeError, wasm->rc kBadPattern=-3).
- html (lexbor vendored): select = trimmed text per matched element, attr = qualified-name attr values (missing attr skips element); no-match/bad HTML = empty, lenient; lxb_selectors_find callback (node, specificity, ctx) collects nodes.

LEXBOR VENDOR NOTES (third_party/lexbor): layout source/lexbor/{html,core,dom,css,selectors,style,ns,tag}; REQUIRED non-obvious bits: css/selectors subtree is the parser, dom-level matching is the separate selectors/ subtree; default allocator impl lives in ports/posix/lexbor/core/memory.c (copied as core/lexbor_memory.c) - without it the static lib links with undefined lexbor_calloc; file(GLOB_RECURSE) needs a reconfigure after adding files.

MIGRATION: mangadex main.lua + fixtures (good/fetcher/fsread) moved to host.* in the same change (in-house plugins, no compat aliases).

GOTCHAS: sol2 as_table does not convert to sol::object - build sol::table manually (1-based keys); Store ctor takes a db PATH not :memory:; after build-dir resets, CMake can consider stale corrupt binaries up-to-date (BAD_COMMAND/Permission denied) - rm the binary and rebuild; multi-config generator mixed with Makefiles leaves binaries in config subdirs - keep single generator.

# Related Concepts
- Related to [Plugin system: folder packaging, manifest, multi-runtime data-contract ABI](../architecture/plugin-system.md)
