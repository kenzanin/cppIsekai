---
type: Reference
title: "WASM guest authoring pitfalls (wasm3, from mangafreak pilot)"
description: "Repeated failure modes when writing cppIsekai WASM guests: shared-scratch clobber, static pool aliasing, reply envelope shape, silent zero on truncation."
tags: [wasm, wasm3, guest, pitfalls, mangafreak, plugin]
generated: { by: agent/mcp, at: "2026-11-13T15:04:43Z" }
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
Host regex results arrive JSON-escaped; a literal \\\" stays in the string. Unescape on copy BEFORE matching quote/quote-adjacent characters, and never truncate mid-escape (dangling backslash) or emit raw bytes <0x20.

## 6. Gas silence
Budget 2G instructions (host-side). A wrong param shape can 404 the fetch and burn the budget on pattern scans over an error page -> trap `out of gas` surfaces only in the daemon log, the API reply is just an error string. Probe with the smallest possible reply first when a path traps.

# Related Concepts
- Related to [goIsekai -> cppIsekai plugin porting matrix](plugin-porting-matrix-goIsekai.md)
