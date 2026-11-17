---
type: constraint
title: "Plugin status strings: canonical vocab, normalized in-plugin"
description: "Every plugin normalizes its site's status soup to Ongoing/Completed/Hiatus/Dropped/Upcoming (else lowercase alnum passthrough, empty->unknown) before emit; host never per-site normalizes."
tags: [plugin, status, normalization, convention]
generated: { by: agent/mcp, at: "2026-11-17T21:50:51Z" }
status: stable
---

## Rule
Plugin scrapers normalize status BEFORE the payload reaches the host. Canonical vocab: `Ongoing | Completed | Hiatus | Dropped | Upcoming`; anything else passes through lowercased-alnum; empty -> `unknown`.

Matching key = lowercase, strip all non-alnum (`ON-GOING`/`on_going`/`Ongoing` -> ongoing; `Ongoin` typo included). Aliases: releasing/publishing->Ongoing, finished->Completed, cancelled/canceled->Dropped, notyetreleased->Upcoming.

## Implementation shape
One `normalize_status()` local per plugin (lua/js) or static helper in the wasm guest - copy-paste parity, no shared host helper (host text.normalize_status stays exact-match infra, plugins do not rely on it).

## History
Raw values leaked into saved_manga (ON-GOING/Ongoin/ongoing mix, 6 distinct values over 30 rows); backfilled by SQL update 2026-11-15. Landed 6e589c0 across mangafreak(wasm)/anisascans/mangafire/atsumaru/mangadex/enrichment-mangadex/mangatown.

# Related Concepts
- [4ucomics plugin: private JSON API v1 behind Cloudflare, thin manga payload](../architecture/4ucomics-plugin.md): 4ucomics returns an empty statuses array for every title, so its status is legitimately outside the canonical vocabulary.
