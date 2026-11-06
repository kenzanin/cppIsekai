---
type: decision
title: "Home demo: library grid, plugin filter, live log"
description: "Home demo live: partial layouts, htmx grid swaps, Crow built-in /static, PORT env; no mustache inheritance in crow"
tags: [ui, logging, demo]
generated: { by: agent/mcp, at: "2026-11-06T11:01:29Z" }
status: stable
---

Home demo shipped. Layout via crow partials (top/bottom/grid_body) -- crow mustache has no {{<layout}} inheritance and partial names need the .html suffix (no auto-append; verified from mustache.h ActionType::Partial + runtime smoke). UI: Tailwind CDN, htmx 4.0.1, alpine 3.17.5 (both vendored into static/ and served via Crow's BUILT-IN /static handler -- a manual route raised 'handler already exists': do not re-register /static). Search is htmx-driven partial swap of #grid via /library/grid. PORT env fallback added: 8080 is occupied by a 'goisekai' process on this host. Acamc tests: plugin filter, q search, page clamp, LIKE escaping (%, _, [ as literal), websocket replay (client got 2 lines).

# Related Concepts
- [cppIsekai core architecture: plugin-per-site manga viewer](manga-viewer-core.md): Home demo is the first slice of the core architecture
