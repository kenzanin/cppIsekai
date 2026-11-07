---
type: reference
title: MangaDex search ranking = followedCount desc
description: "Search mangadex.org = order[followedCount]=desc di API; relevance enum ada tapi tidak menjamin exact-match first; plugin mengirim followedCount dan menerima title/query/q."
tags: [mangadex, search, ranking]
generated: { by: agent/mcp, at: "2026-11-07T03:11:47Z" }
status: stable
---

## Fact (verified live 2026-11-07)

MangaDex API `GET /manga?title=...` tanpa `order` memakai server-default order yang bury hasil judul persis. Ranking yang menghasilkan situs-web seperti website mangadex.org (q=one piece → One Piece #1) adalah:

```
order[followedCount]=desc
```

`order[relevance]=desc ada di enum tapi hasilnya tidak konsisten menaruh judul exact match di urutan 1.

Terbukti di plugin: `plugins_dev/lua/mangadex/main.lua` search mengirim `order[followedCount]=desc dan menerima `params.title` / `query` / `q` (spec param = `title`).

# Related Concepts
- [MangaDex auth and API quirks (verified)](mangadex-auth-quirks.md): Same verified-behaviors family: ranking quirk completes the auth quirks reference.
