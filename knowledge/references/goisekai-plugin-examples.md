---
type: reference
title: "goIsekai reference corpus: example plugins + HAR captures"
description: "goIsekai (~/tmp/goIsekai) was the user's first golang experiment (test harness + stack trial); its example plugins and HAR captures are the reference corpus for writing cppIsekai site plugins."
tags: [plugin, goisekai, reference, har, examples]
generated: { by: agent/mcp, at: "2026-11-11T03:37:47Z" }
status: stable
---

## goIsekai reference corpus (user's first attempt, Go)

- Repo: `~/tmp/goIsekai` — golang experiment used to test harness + tech stack; **cppIsekai is the cleaner, more effective successor**. Not a dependency, reference only.
- **Example plugins**: `~/tmp/goIsekai/examples/plugins/<runtime>/<site>/` — runtimes: `lua/` (dummy, mangzio), `js/` (~40 sites: mangadex, mangapill, mangafire, 1manga, anisascans, atsumaru, batcave, comico, comik, comix, comizy, dynastyscan, fenyk, kaliscan, lhtranslation, madaradex, mangabay, mangabuddy, mangade, mangatown, sanascans, weebcentral, zeroscann, …), `wasm/` (1manga, dummy, madarascans, roliascan), `native/` (~30+ incl. odin variants: atsumaru_odin, comicasura_odin, …), `yaegi/` (1manga, atsumaru, …).
- **HAR captures**: `~/tmp/goIsekai/tmp/*.har` (many at repo root, e.g. comix.har) and `~/tmp/goIsekai/tmp/har/` (9 files: comik 117MB, comix, library_filter, manga_detail_action 151KB, mangahere, mangapill, mangaread 110MB, sanascans, weebcentral). Root `tmp/` also holds aidoku/, extension-source/, comix-downloader-src/, tinygo-hello/.
- Use cases: port/verify site plugin behavior (search/detail/chapter/pages/covers) against real traffic; compare runtime approaches; mine selectors/endpoints. cppIsekai plugin ABI is JSON-text-in/out with host tools (http.fetch, storage.*, log.*) — port logic, not Go code.
- Note: goIsekai also has `knowledge/` (own OKF bundle) and openspec/ — do not merge bundles; treat as read-only reference. Paths verified 2026-11-11.
