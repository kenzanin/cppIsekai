---
type: decision
title: "cppIsekai core architecture: plugin-per-site manga viewer"
description: "Plugin-per-site manga viewer: plugins parse HTML themselves, host = crow+mustache + libsql + impersonate proxy w/ disk cache; light sandbox; zero new deps"
tags: [architecture, plugin, manga]
generated: { by: agent/mcp, at: "2026-11-07T14:04:38Z" }
status: stable
---

Product: local manga reader/aggregator daemon.

UI: server-rendered, crow + crow::mustache (verified in 1.3.4), templates loaded from disk.
Persistence: libsql (sqlite amalgamation, static, WAL).

Scraper model: 1 plugin = 1 site; plugins orchestrate host tools. RESOLVED (user): HTML parsing happens INSIDE the plugin -- lua patterns/pure-lua libs, wasm bundles its own parser; host stays fetch-only (no lexbor, zero new C++ deps).
Plugin runtimes: lua 5.4 via sol2 AND wasm via wasm3. RESOLVED (user): trust = self-authored plugins, light sandbox only (whitelisted lua env + debug hook timeout).

Host tool surface (plugin ABI): http.get (curl-impersonate, per-site fingerprint via curl_easy_impersonate -- extern decl required, see references/dependency-api), db.query/db.exec (prepared stmts), log. RESOLVED (user): images proxied by host WITH disk cache (/img?url=... route, fingerprint+referer handled host-side).

Concurrency: BS::thread_pool workers; per-worker lua_State / wasm runtime instance (no sharing across threads); moodycamel::ReaderWriterQueue for background refresh jobs.
Plugin manifest: glaze-parsed toml/json -- id, name, base_url, fingerprint, entrypoint, version.

New C++ dependencies: NONE -- existing set covers everything.
Deferred (ponytail): test framework at first plugin regression suite; WAMR swap if wasm3 interpreter perf hurts; 1-file pure-lua json/html helpers vendored into plugin assets on first lua plugin that needs them; libarchive only if CBZ export appears.

# Related Concepts
- [Dependency vendoring: system-shared first, minimal vendoring](../decisions/third-party-dependencies.md): Core architecture consumes the vendored dependency set decided here
- [cppIsekai dependency API surface](../references/dependency-api.md): Plugin ABI function names come from the verified dependency API reference
- Related to [cppIsekai image-cache + offline-first pathway](image-cache-offline.md)
- [cli-config: registry-backed config surface (CLI/env/ini/dump/save)](cli-config.md): Config layer feeds the host core (paths/port wiring to store, plugin, cache); registry is consumer of its dependency APIs.
