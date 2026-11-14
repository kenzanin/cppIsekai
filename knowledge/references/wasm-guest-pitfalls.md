---
type: Reference
title: "WASM guest authoring pitfalls (wasm3, from mangafreak pilot)"
description: "Repeated failure modes when writing cppIsekai WASM guests: shared-scratch clobber, static pool aliasing, reply envelope shape, silent zero on truncation, missing __wasm_call_ctors init (no start fn => null-table trap), raw query-key params (q not title)."
tags: [wasm, wasm3, guest, pitfalls, mangafreak, plugin]
generated: { by: agent/mcp, at: "2026-11-14T03:42:21Z" }
status: stable
---

# WASM guest authoring pitfalls

Lessons from the mangafreak pilot (2026-11-13), each bug cost a debug cycle:

## 1. Shared scratch clobber inside iteration loops
Host regex helpers (`find`) write results into the single 2 MiB `TMP`. Calling `find` while still iterating a previous `find_n` result that lives in `TMP` destroys the parent match list mid-walk -> later iterations parse garbage or stop early.
**Rule:** results consumed inside a loop must live in a different buffer than the loop's source. Guest keeps `TMP` (find_all results) and `TMP2` 64 KiB (per-item find via `find2`).

## 2. Static pool aliasing across loop iterations
`json_elem` rotates a fixed pool of return buffers. With per-iteration extractions (block, href, title, slug, cover) the pool wraps every ~1.5 iterations and a buffer extracted in iteration N is silently overwritten in N+1/N+2 while still referenced at emit time. Symptoms: intermittent empty results depending on iteration count.
**Rule:** anything held past the next extraction (id/title/slug emitted or deduped) must be copied to per-iteration storage (static rotating bufs keyed by iteration, or memcpy before next call).

## 3. Reply envelope must match the house shape
Search UI reads `data.items` + `data.total`. A guest replying a bare list renders as zero items in the UI even though the plugin endpoint works. House envelope: `{"ok":true,"data":{"items":[...],"total":N}}` for search; plain arrays for detail sub-lists per existing plugins.

## 4. Oversized results are silent zeros
Host imports map oversized results to `kTruncated` which guest wrappers coerce to 0: an undersized guest buffer reads as "no matches"/"empty page", never as an error. Size scratch from real payload (mangafreak: 650 KB page, 1.3 MB match JSON) and set `-Wl,--initial-memory` explicitly.

## 5. Escape-then-match ordering
Host regex results arrive JSON-escaped; a literal \" stays in the string. Unescape on copy BEFORE matching quote/quote-adjacent characters, and never truncate mid-escape (dangling backslash) or emit raw bytes <0x20.

## 6. Gas silence
Budget 8G instructions per call (host-side `kGasPerCall`; one_piece-scale chapter lists burn >2G, hence the 2G->8G bump). A wrong param shape can 404 the fetch and burn the budget on pattern scans over an error page -> trap `out of gas` surfaces only in the daemon log, the API reply is just an error string. Probe with the smallest possible reply first when a path traps.

## 7. No start section: guests run __wasm_call_ctors themselves
The wasm3 host load path (src/plugin/wasm/wasm_load.cpp) runs parse/load/RunStart/link/compile and never `__wasm_call_ctors`; `--no-entry` guests ship no start section. A C++ guest (wasi libc++) then traps `wasm: [trap] uninitialized element` on the first `call_indirect` - crt/table init lives in the ctors, so the table slot is still null.
**Rule:** every wasm guest runs `__wasm_call_ctors()` exactly once, lazily, from `alloc()` (host always calls it first); see plugins_dev/wasm/mangafox/main.cpp (2026-11-14, commit 848a8ac). Harmless for C guests.

## 8. Envelope params carry the RAW query keys
The call envelope params mirror the HTTP query verbatim: `?q=...` arrives as `"q":"..."`; `title`/`query` keys do not exist unless the caller sent them. Guests reading only title/query see EMPTY search args: mangafox silently searched the default listing (looked alive, query ignored), mangafreak fetched `BASE /Find/` -> 404 -> `fetch failed`.
**Rule:** read the raw key first, house names as fallback - `q` then `title`/`query` for search, `id` for detail/chapters/pages. See plugins_dev/wasm/mangafox + mangafreak handle() (2026-11-14, commits 6aaa57e/0cb4493).

# Related Concepts
- Related to [goIsekai -> cppIsekai plugin porting matrix](plugin-porting-matrix-goIsekai.md)
