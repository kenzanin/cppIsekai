---
type: decision
title: "Plugin system: folder packaging, manifest, multi-runtime data-contract ABI"
description: "Plugin packaging: one folder per plugin under data/plugins (entry + manifest json + logo), declared via ini; ABI = JSON-text-in/out data contract + per-runtime host-tool bindings; jsonb is DB-side storage only, never the wire format; lua first, wasm/js/micropython later"
tags: [plugin, abi, packaging, jsonb]
generated: { by: agent/mcp, at: "2026-11-07T11:37:17Z" }
status: stable
---

Plugin system packaging + multi-runtime ABI design (no implementation yet).

## Storage layout (user-specified)
Path declared in data/cppisekai.ini: plugins=data/plugins. One folder per plugin:

  data/plugins/mangadex/
    main.lua          # entry point (runtime-extensions later: main.wasm, main.js, main.py)
    main.json         # manifest: name, version, runtime (lua|wasm|js|micropython), capabilities (search/detail/chapters/enrichment), entry, dynamic plugin config
    logo.png|logo.svg|logo.jpg|logo.avif   # shown everywhere the plugin name appears (sidebar, filters, etc.)

Logo served via /plugin/<id>/logo with content-type by extension.

## ABI: data contract, not native ABI
- Plugins declare capabilities in main.json; the host calls them through data-in/data-out (JSON in, JSON out) -- never through native function signatures.
- Host-tools (http.fetch via curl-impersonate, dom.select, db, log, now) are one specification exposed through per-runtime bindings; adding a runtime = adding a binding, zero contract changes.
- Trade-off: per-call JSON serialization overhead -- negligible for scrape flows, bought flexibility is worth it.
- Runtime priority: lua first (sol2 linked, sandbox whitelist + timeout hook per the light-sandbox decision), wasm3 second, then js (QuickJS candidate, small/embeddable) and micropython if attractive.
- Loader: scan plugins dir at startup, parse main.json, register into plugin table.

## Wire vs storage format (clarified)
- Plugin<->host wire format: plain JSON TEXT, always. JSONB is a SQLite STORAGE format (binary blob), not an interchange format; lua/wasm/js/micropython all only speak text JSON. Sending jsonb blobs to plugins would force them to convert to text anyway -- pure extra step.
- Optimal jsonb point: inside Store. Persist: plugin returns JSON text -> host binds text -> SQLite stores via jsonb() once (binary, pre-parsed). Read: host jsonb_extract(data,'$') -> text -> glaze parse -> struct. Partial queries (jsonb_extract(data,'$.alt_title')) cheaper on the pre-parsed blob. jsonb_valid() usable as a cheap Store test assertion.
- Boundary shape: plugin --JSON text--> host --jsonb()--> SQLite; back out via jsonb_extract -> text -> plugin.

Relations: implements the plugin orchestration in manga-viewer-core; consumes normalization-contract (plugins normalize to MangaDex format + unix time); config file location follows the SimpleIni setup.

# Related Concepts
- [Normalisation contract: MangaDex canonical + chapter release_at](normalization-contract.md): Wire-vs-storage clarification extends the unix-time + JSON policy (okf_update wipes relations)
- [cppIsekai core architecture: plugin-per-site manga viewer](manga-viewer-core.md): Plugin system realizes the host-tools orchestration of the core architecture
- [Dependencies & plugins: curl-first APIs, runtime parity, no hand-rolled crypto](../decisions/deps-and-plugins.md): Runtime parity: quickjs-ng menjadi runtime kedua dengan feature set & signature sama (data-contract ABI tetap; wasm3 tidak diprioritaskan).
