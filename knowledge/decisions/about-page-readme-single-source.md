---
type: decision
title: About page renders README.md live
description: "The /about page fetches the repo-root README.md per-request (/about/readme, no-store) and renders it client-side with vendored marked + mermaid; README.md is the single source for GitHub and the app, with mermaid fenced blocks for diagrams."
tags: [about, readme, mermaid, architecture]
generated: { by: agent/mcp, at: "2026-11-15T14:35:11Z" }
status: stable
---

- Routes: /about (about.mustache), /about/readme (raw README.md, text/plain, Cache-Control no-store, read per-request).
- Assets: static/marked.min.js.br (marked 18.2.0, jsdelivr terser build) and static/mermaid.min.js.br (mermaid 12.3.0), served via register_static's br_asset pattern; both fetched from cdn.jsdelivr.net and brotli-packed.
- README.md rewritten with current status, architecture mermaid flowchart, DB erDiagram (logical keys; only enforced FK is title.plugin_id -> plugin.id); mermaid renders natively on GitHub.
- Inline-CSS mustache page (zinc theme); fetch/marked failure shows a Retry banner; mermaid failure prepends a yellow notice instead of clobbering content.
- Rationale: no new C++ markdown dependency; single source for GitHub + app; edits need no rebuild (F5 only).

# Related Concepts
- [Static assets: vendored, precompressed brotli-only via explicit routes](static-assets-br.md): About page renders markdown from the repo-root README.md using the same brotli-only vendored static asset serving decisions/static-assets-br defines (marked.min.js.br, mermaid.min.js.br via br_asset routes).
