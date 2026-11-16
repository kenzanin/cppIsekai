---
type: decision
title: "Weeb Central ported as Lua plugin (plain-HTML, CF-safe)"
description: weebcentral.com ported to a Lua plugin and verified e2e; other HAR candidates rejected on liveness/CF grounds.
tags: [plugin, weebcentral, scrape, port]
generated: { by: agent/mcp, at: "2026-11-16T13:42:05Z" }
status: stable
---

HAR audit (tmp/todaymanga.har, webnex.har, xbato.har, weebcentral.har) on 2026-11-16; user scope: 'pilih yang hidup, port yang CF-aman'.

Ported: weebcentral.com (plugins_dev/lua/weebcentral/, symlinked into data/plugins/). Plain server-rendered HTML, no CF challenge. Endpoints: /search/data (32-item offset pages), /search (tag facets scraped from hidden inputs for genres), /series/<ULID>, /series/<ULID>/full-chapter-list, /chapters/<ULID>/images (bare path; query params 307 to /400). Stable id = ULID. Search interface: linear offset paging via page param.

Verified e2e against local daemon (2026-11-16): search (32 items, tags/statuses/years), detail (authors/tags/desc/status), chapters (86 rows, ISO created_at), pages (38 JPEGs) + /cache/img transcode, genres (39 unique tags), logo 200.

Rejected for now:
- todaymanga.com: CF edge returns 502 (todaymanga unreachable). Would also need GraphQL persisted-query protocol (finding todaymanga-host-apis... not yet written).
- xbato.site: connection timeout; capture shows 9x challenges.cloudflare.com + client-side image decrypt in a JS bundle (finding xbato-image-decrypt). Not CF-safe.
- webnex.cc: live (Next.js) but results are RSC/Flight streamed client-side (search shell 227KB; ?_rsc= hits got 403 in capture); 2 cdn-cgi challenge-platform hits in same HAR. Not CF-safe; a port would need RSC protocol work or the lightpanda fetch engine, not plain fetch.

webnex notes for future attempt: /browse?q=isekai 200 contains /catalog/v1/covers/* asset CDN links; /read/<id>/pages returns application/json (1683B in capture); detail /manga/<slug>; reader /read/<id>.
