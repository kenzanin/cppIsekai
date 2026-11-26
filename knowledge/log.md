## 2026-11-26
* **Relate** [2026-11-26T06:02:29Z]: Linked `decisions/store-raw-handle-reuse.md` to `decisions/db-read-parallel.md` (keeps write path single-writer; read-parallel still deferred by this decision).
* **Relate** [2026-11-26T06:02:29Z]: Linked `decisions/store-raw-handle-reuse.md` to `findings/store-serialized-ops.md` (refines the single-connection store: raw handle reuse for api_cache ops).
* **Creation** [2026-11-26T06:02:25Z]: Documented concept `decisions/store-raw-handle-reuse.md` (Store: reuse one raw sqlite handle for api_cache (measured 0.4ms/call saved)).
* **Update**: Linked `findings/harimanga-cf-challenge-2026-11-26.md` to `architecture/plugin-system.md` (image_ua manifest field + jar bridge extend the host plugin surface).
* **Update**: Linked `findings/harimanga-cf-challenge-2026-11-26.md` to `findings/browser-session-waf-2026-11-17.md` (harimanga repeats the comix WAF pattern; challenge-wait + external jar seed, no daemon cookies tool).
* **Update**: Linked `findings/harimanga-cf-challenge-2026-11-26.md` to `architecture/plugin-system.md` (cookies tool + image_ua extend the host surface used by the plugin system).
* **Update**: Linked `findings/harimanga-cf-challenge-2026-11-26.md` to `findings/browser-session-waf-2026-11-17.md` (harimanga repeats the comix WAF pattern with the app-local chrome; adds cookie/UA bridge for image path).
* **Update**: Updated concept `findings/harimanga-cf-challenge-2026-11-26.md`.

## 2026-11-25
* **Update**: Linked `findings/harimanga-cf-challenge-2026-11-26.md` to `architecture/plugin-system.md` (cookies tool + image_ua extend the host surface used by the plugin system).
* **Update**: Linked `findings/harimanga-cf-challenge-2026-11-26.md` to `findings/browser-session-waf-2026-11-17.md` (harimanga repeats the comix WAF pattern with the app-local chrome; adds cookie/UA bridge for image path).
* **Creation**: Documented concept `findings/harimanga-cf-challenge-2026-11-26.md` (harimanga CF challenge: browser fetch + jar/UA bridge).
* **Update**: Linked `decisions/plugin-show-off-deferred.md` to `architecture/plugin-system.md` (visibility flag would extend the plugin manifest/host surface; deferred by this decision).
* **Update**: Updated concept `decisions/plugin-show-off-deferred.md`.
* **Creation**: Documented concept `decisions/plugin-show-off-deferred.md` (plugin show/hide ditunda: toggle enabled cukup).
* **Update**: Updated concept `architecture/keroscans-plugin.md`.
* **Update**: Linked `architecture/keroscans-plugin.md` to `architecture/plugin-system.md` (keroscans plugin instance of the folder/manifest/ABI contract).
* **Creation**: Documented concept `architecture/keroscans-plugin.md` (keroscans plugin: endpoints + decimal-subchapter gotcha).
* **Update**: Linked `decisions/enrich-module-location.md` to `architecture/detail-page.md` (enrich module implements the detail-page enrichment flow moved out of http/).
* **Creation**: Documented concept `decisions/enrich-module-location.md` (enrich module lives in src/enrich/, http/ = routes + shaping).
* **Creation**: Documented concept `findings/mangagoku-cdn-partial-migration-2026-11-25.md` (mangagoku partial CDN migration: reader images split 2xstorage webp / waitst jpg).
* **Update**: Linked `findings/plugin-seo-tag-pills-2026-11-25.md` to `architecture/manhwato-plugin.md`.
* **Creation**: Documented concept `findings/plugin-seo-tag-pills-2026-11-25.md` (Site SEO tag farms must not become detail tags; use JSON-LD genres).
* **Update**: Updated concept `decisions/image-referer-domain-match.md`.
* **Update**: Linked `findings/plugin-script-module-split-2026-11-25.md` to `decisions/source-layout-policy.md`.
* **Creation**: Documented concept `findings/plugin-script-module-split-2026-11-25.md` (All Lua/JS plugin scripts split into per-action modules; helper duplication census).
* **Update**: Updated concept `findings/offline-cached-chapter-2026-11-25.md`.
* **Update**: Linked `findings/offline-cached-chapter-2026-11-25.md` to `architecture/chapter-state-flags.md`.
* **Creation**: Documented concept `findings/offline-cached-chapter-2026-11-25.md` (Offline cached chapters: /api/pages must fall back to chapter rows, not the plugin envelope).

## 2026-11-24
* **Update**: Linked `architecture/manhwato-plugin.md` to `architecture/plugin-system.md`.
* **Creation**: Documented concept `architecture/manhwato-plugin.md` (Manhwato Lua plugin: SSR HTML via the daemon's Chrome bridge).
* **Update**: Linked `findings/comik-search-pagination-2026-11-25.md` to `findings/lua-plugin-audit-2026-11-16.md`.
* **Creation**: Documented concept `findings/comik-search-pagination-2026-11-25.md` (comik search ignored ?page -- every UI page returned the same 48 cards).
* **Update**: Linked `architecture/mangade-plugin.md` to `architecture/plugin-system.md`.
* **Creation**: Documented concept `architecture/mangade-plugin.md` (MangaDE Lua plugin: JSON API contract from tmp/mangade.har).
* **Update**: Linked `architecture/readcomiconline-plugin.md` to `decisions/plugin-lifecycle.md`.
* **Update**: Linked `findings/chapter-flags-route-split-404.md` to `decisions/source-layout-policy.md`.
* **Creation**: Documented concept `findings/chapter-flags-route-split-404.md` (TU split silently dropped POST /chapter/flags (404) -- route-surface test added).
* **Update**: Updated concept `architecture/detail-page.md`.
* **Update**: Linked `architecture/reader-page-progress.md` to `architecture/shareable-url-state.md`.
* **Creation**: Documented concept `architecture/shareable-url-state.md` (Every page mirrors its view state into the URL (reader parity, replaceState only)).
* **Update**: Updated concept `architecture/extension-source-four-ports.md`.
* **Update**: Updated concept `architecture/mangak-mangagoku-plugins.md`.
* **Update**: Updated concept `architecture/chapter-state-flags.md`.
* **Update**: Linked `architecture/extension-source-four-ports.md` to `references/plugin-porting-matrix-goIsekai.md` (four new ports from extension-source, not examples/plugins).
* **Creation**: Documented concept `architecture/extension-source-four-ports.md` (Four extension-source ports: Asmodeus, Bbato, Dynasty, MangaForFree).
* **Update**: Updated concept `decisions/db-read-parallel.md`.
* **Update**: Linked `architecture/mangak-mangagoku-plugins.md` to `decisions/image-referer-domain-match.md` (mangak.io and mangagoku.com image CDNs use manifest image_referer / same-site referer).
* **Update**: Linked `architecture/mangak-mangagoku-plugins.md` to `architecture/plugin-system.md` (new Lua ports following the folder/manifest/JSON-text ABI).
* **Creation**: Documented concept `architecture/mangak-mangagoku-plugins.md` (MangaK + MangaGoku ports, and shared MangaHub engine).
* **Update**: Linked `packaging/desktop-launcher.md` to `architecture/cli-config.md` (desktop Exec reuses the canonical --config launcher instead of duplicating boot flags).
* **Creation**: Documented concept `packaging/desktop-launcher.md` (.desktop launcher: templated Exec/Path/Icon + install script).
* **Update**: Updated concept `decisions/in-memory-assets-zip.md`.
* **Update**: Updated concept `architecture/readcomiconline-plugin.md`.
* **Update**: Linked `architecture/readcomiconline-plugin.md` to `decisions/image-referer-domain-match.md` (CDN images rely on host domain-matched Referer, no manifest override).
* **Update**: Linked `architecture/readcomiconline-plugin.md` to `architecture/plugin-system.md` (new Lua port following the folder/manifest/JSON-text ABI; no runtime changes).
* **Creation**: Documented concept `architecture/readcomiconline-plugin.md` (Read Comics Online plugin: HAR-derived Blade scraper + bound CF clearance).

## 2026-11-23
* **Update**: Updated concept `decisions/in-memory-assets-zip.md`.
* **Update**: Linked `decisions/in-memory-assets-zip.md` to `architecture/cli-config.md` (adds one registry knob with a new readonly flag surfaced through describe() and GET /config).
* **Update**: Linked `decisions/in-memory-assets-zip.md` to `decisions/static-assets-br.md` (supersedes disk-only brotli asset serving with an optional in-memory zip/folder root; .br bodies are unchanged).
* **Creation**: Documented concept `decisions/in-memory-assets-zip.md` (Assets: optional in-memory zip/folder root with read-only ini knob).
* **Update**: Linked `findings/enrichment-search-title-mismatch.md` to `decisions/library-ownership-model.md` (Follow-up: the orphan gate must treat loaded-but-unregistered enrichment runtimes as live.).
* **Creation**: Documented concept `findings/enrichment-search-title-mismatch.md` ('Capped at 5 covers' was a stale candidate plus a title the source does not index).
* **Update**: Linked `findings/browse-writes-nothing-orphan-free.md` to `decisions/library-ownership-model.md` (Evidence for the implemented model: 73% of chapter rows were browse dust before the routing change.).
* **Creation**: Documented concept `findings/browse-writes-nothing-orphan-free.md` (Routing unsaved pairs to memory removes the orphan class entirely (measured 73%)).
* **Update**: Updated concept `decisions/library-ownership-model.md`.
* **Update**: Linked `decisions/library-ownership-model.md` to `decisions/library-promote-on-save.md` (Supersedes the where-to-stage question: the staging store is process memory, not api_cache, and promotion carries pre-save read state.).
* **Creation**: Documented concept `decisions/library-ownership-model.md` (Library ownership: RAM for browse, SQLite for saved, plugin memory marked orphan (planned, openspec library-ownership)).
* **Update**: Linked `findings/client-painted-classes-fouc.md` to `decisions/library-promote-on-save.md` (Same batch: the grid wipe/SSR work surfaced the FOUC; dim class now server-rendered from status_flags.).
* **Creation**: Documented concept `findings/client-painted-classes-fouc.md` (Client-side class painting after a fetch = FOUC; SSR the class instead).
* **Update**: Updated concept `decisions/library-promote-on-save.md`.
* **Update**: Linked `decisions/library-promote-on-save.md` to `decisions/api-cache-null-payload.md` (Promotion treats a NULL/empty payload row as nothing cached (no-op), same miss rule.).
* **Update**: Linked `decisions/library-promote-on-save.md` to `architecture/local-first-reads.md` (Closes the gap where a saved title's chapters still depended on api_cache age for local-first reads.).
* **Update**: Linked `decisions/library-promote-on-save.md` to `decisions/saved-manga-detail-source-of-truth.md` (Extends it: the detail row is now materialized at save time instead of on the first open after save.).
* **Creation**: Documented concept `decisions/library-promote-on-save.md` (Library promotion: api_cache is the staging store; chapters become a read source (planned, openspec library-promote-on-save)).

## 2026-11-21
* **Update**: Updated concept `decisions/template-golden-test.md`.
* **Update**: Updated concept `architecture/atsumaru-plugin.md`.
* **Update**: Linked `decisions/image-referer-domain-match.md` to `decisions/host-text-entity-fixpoint.md` (Both keep host-side cross-runtime behavior data-driven; image referer now follows the URL domain instead of the caller plugin.).
* **Creation**: Documented concept `decisions/image-referer-domain-match.md` (Image referer follows the URL's own site domain).
* **Update**: Linked `decisions/ui-action-feedback.md` to `decisions/host-text-entity-fixpoint.md` (Both are shared-core contracts: ui-action-feedback keeps action feedback in one template partial exactly as host-text-entity-fixpoint keeps text cleanup in one host core; single-source-of-truth over per-site copies.).
* **Creation**: Documented concept `decisions/ui-action-feedback.md` (Shared toast + LED action-feedback contract (feedback.mustache; no new lib/route)).
* **Update**: Linked `decisions/plugin-host-promotion.md` to `architecture/plugin-system.md` (Host promotion is the mechanism behind the plugin system's shared host-tool surface: cores live in src/plugin, runtimes keep thin marshals.).
* **Update**: Linked `decisions/host-text-entity-fixpoint.md` to `decisions/plugin-host-promotion.md` (Extends the plugin-host-promotion series: entity decoding/display cleanup is one more shared host core instead of per-plugin code (mangabuddy's private deep_unescape was the last copy).).
* **Creation**: Documented concept `decisions/plugin-host-promotion.md` (Plugin host promotion: shared host cores, thin runtime marshals).
* **Creation**: Documented concept `decisions/host-text-entity-fixpoint.md` (host.text.clean: lexbor entity fixpoint + RE2/utf8proc numeric sweep).
* **Update**: Linked `findings/kit-impact-review-smoke.md` to `findings/store-migrate-source-callers.md` (Same call-site inventory, more detail; this finding records the graph-index gap that blocked the tooling path.).
* **Creation**: Documented concept `findings/kit-impact-review-smoke.md` (Store::migrate_source inbound call sites (graph smoke review)).
* **Update**: Linked `findings/store-migrate-source-callers.md` to `decisions/library-migrate-source.md` (Impact analysis of the migrate_source entry point recorded by the worker; relates to the migrate-source design decision.).
* **Creation**: Documented concept `findings/store-migrate-source-callers.md` (Store::migrate_source inbound call sites).
* **Creation**: Documented concept `decisions/suwayomi-webui-contract-first.md` (Suwayomi WebUI adoption: contract first, no Go port).

## 2026-11-20
* **Creation**: Documented concept `decisions/library-migrate-source.md` (Library migrate-source: repoint, read-boundary carry).
* **Update**: Linked `decisions/library-pin.md` to `decisions/saved-manga-detail-source-of-truth.md`.
* **Creation**: Documented concept `decisions/library-pin.md` (Pinned titles: a flag on saved_manga, rendered as its own grid section).
* **Update**: Linked `decisions/reader-keyboard-nav.md` to `architecture/reader-fullscreen.md`.
* **Creation**: Documented concept `decisions/reader-keyboard-nav.md` (Reader keyboard map: arrows pan, page only at the edge).
* **Creation**: Documented concept `decisions/clang-doc-generation.md` (clang-doc generation: one process, src-only, md needs vendored templates).

## 2026-11-19
* **Creation**: Documented concept `findings/mangahub-graphql-move.md` (mangahub.io GraphQL moved to the site host, cookie auth, NSFW flag).
* **Update**: Linked `findings/plugin-liveness-2026-11-19.md` to `decisions/deps-and-plugins.md`.
* **Creation**: Documented concept `findings/plugin-liveness-2026-11-19.md` (Plugin liveness scan: only kaliscan.me and anisascans.in are globally dead; most peers fail locally on ISP DNS interception).
* **Update**: Linked `decisions/sanitizer-build.md` to `decisions/xmake-build-system.md`.
* **Creation**: Documented concept `decisions/sanitizer-build.md` (Sanitizers are an opt-in debug build (--sanitize), never the release daemon).
* **Update**: Linked `constraints/dle-json-float-ids.md` to `constraints/plugin-image-url-regex.md`.
* **Update**: Linked `constraints/dle-json-float-ids.md` to `findings/js-image-referer-gap.md`.
* **Creation**: Documented concept `constraints/dle-json-float-ids.md` (DLE sites: JSON numeric ids must be formatted integrally, and image CDNs need the trailing-slash Referer).
* **Creation**: Documented concept `decisions/tray-sni-icon-resolution.md` (Tray icon: SNI IconName must be an absolute path for xfce4-panel's status tray).
* **Update**: Linked `decisions/search-read-model-cache.md` to `decisions/search-envelope-page-count.md`.
* **Creation**: Documented concept `decisions/search-envelope-page-count.md` (Search envelope: plugin reports real total + optional data.pages; host pager prefers it).
* **Update**: Updated concept `decisions/xmake-build-system.md`.

## 2026-11-18
* **Update**: Updated concept `decisions/search-read-model-cache.md`.
* **Update**: Updated concept `decisions/chapter-zero-falsy-regression.md`.
* **Update**: Linked `decisions/plugin-website-field.md` to `constraints/plugin-image-url-regex.md`.
* **Update**: Linked `decisions/plugin-website-field.md` to `findings/js-image-referer-gap.md`.
* **Update**: Updated concept `decisions/plugin-website-field.md`.

## 2026-11-17
* **Update**: Updated concept `architecture/cache-layout-v3.md`.
* **Update**: Linked `decisions/drop-date-dependency.md` to `findings/plugin-date-shapes-2026-11-18.md` (Host date conversion is stdlib-only; the same epoch-vs-ISO split shows up at the template boundary.).
* **Update**: Linked `findings/re2-findall-one-indexed.md` to `constraints/plugin-image-url-regex.md` (The extension group is m[2] because host re2 tables are 1-based -- the indexing fact this regex rule depends on.).
* **Update**: Linked `decisions/renderer-fetch-engines.md` to `architecture/4ucomics-plugin.md` (4ucomics requires the lightpanda fetch profile for every call; plain curl gets the Cloudflare challenge.).
* **Update**: Linked `constraints/plugin-status-canonical-vocab.md` to `architecture/4ucomics-plugin.md` (4ucomics returns an empty statuses array for every title, so its status is legitimately outside the canonical vocabulary.).
* **Update**: Linked `constraints/host-chapter-state-keys.md` to `architecture/chapter-state-flags.md` (Host-side chapter state keys are the identity contract the flags bitfield rides on.).
* **Update**: Linked `findings/store-serialized-ops.md` to `architecture/chapter-state-flags.md` (The chapter-state store methods are what exposed the dangling-transaction wedge; TxGuard now wraps them.).
* **Update**: Linked `architecture/chapter-state-flags.md` to `constraints/plugin-chapter-number-placeholders.md` (Why manga-wide position bitmaps were rejected: chapter numbers are untrusted, duplicated and non-unique.).
* **Update**: Linked `architecture/chapter-state-flags.md` to `architecture/status-flags-bitfield.md` (Chapter flags follow the same bitfield convention as saved_manga.status_flags.).
* **Update**: Linked `findings/plugin-probe-param-keys.md` to `architecture/manga-detail-page.md` (call_cached serves rows at any age, so a bad probe's cache row behaves like a stale-serve hit.).
* **Update**: Linked `constraints/plugin-image-url-regex.md` to `findings/lua-plugin-audit-2026-11-16.md` (The webp-only regex was found after the audit's plugin sweep; same failure shape as the regex bugs listed there.).
* **Update**: Linked `architecture/4ucomics-plugin.md` to `findings/valid-title-pair-charset.md` (4ucomics' 202-char slug is what forced the valid_title_pair cap to 255.).
* **Update**: Linked `findings/plugin-date-shapes-2026-11-18.md` to `architecture/4ucomics-plugin.md` (4ucomics is the ISO-date emitter whose dates were blank on the detail page.).
* **Update**: Linked `findings/plugin-date-shapes-2026-11-18.md` to `architecture/normalization-contract.md` (Recorded divergence: plugins emit ISO date strings as well as epoch seconds; the template boundary accepts both.).
* **Creation**: Documented concept `architecture/chapter-state-flags.md` (Chapter state: one flags bitfield per chapter row, chapters as metadata hub).
* **Creation**: Documented concept `findings/plugin-probe-param-keys.md` (Plugin probes with wrong param keys write a cache row that shadows the real handler).
* **Creation**: Documented concept `constraints/plugin-image-url-regex.md` (Plugin page-image regexes must match every image extension a site serves).
* **Creation**: Documented concept `architecture/4ucomics-plugin.md` (4ucomics plugin: private JSON API v1 behind Cloudflare, thin manga payload).
* **Creation**: Documented concept `findings/plugin-date-shapes-2026-11-18.md` (Detail page dates: plugins emit both epoch seconds and ISO strings).
* **Update**: Updated concept `findings/valid-title-pair-charset.md`.
* **Update**: Updated concept `findings/store-serialized-ops.md`.
* **Update**: Linked `findings/browser-chrome-backend-2026-11-17.md` to `decisions/comix-turnstile-token-2026-11-17.md` (Chrome swap made turnstile minting moot: real-Chrome engine solves the WAF transparently; decision left deprecated.).
* **Update**: Linked `findings/browser-chrome-backend-2026-11-17.md` to `findings/browser-host-apply-2026-11-17.md` (Supersedes the lightpanda-era backend; its invariants (D13, single-session) carry over re-shaped.).
* **Creation**: Documented concept `findings/browser-chrome-backend-2026-11-17.md` (Chrome/CDP browser backend: invariants, wait tool, and the comix tomy2x redeploy breakage).
* **Update**: Updated concept `decisions/comix-adoption-route-2026-11-16.md`.
* **Update**: Updated concept `decisions/comix-turnstile-token-2026-11-17.md`.
* **Creation**: Documented concept `decisions/comix-turnstile-token-2026-11-17.md` (comix turnstile token: how to mint the WAF cookie for headless harvest (DRAFT - for discussion)).
* **Update**: Updated concept `findings/browser-session-waf-2026-11-17.md`.
* **Creation**: Documented concept `findings/browser-session-waf-2026-11-17.md` (comix WAF turnstile unsolvable in lightpanda headless; session ops notes).

## 2026-11-16
* **Creation**: Documented concept `findings/browser-host-apply-2026-11-17.md` (browser-host apply findings (reproc reuse EINVAL, lightpanda await)).
* **Creation**: Documented concept `findings/reader-spill-cached-overcount.md` (Reader prefetch spill wrote next-chapter pages into the current chapter dir (cached>total)).
* **Update**: Updated concept `decisions/comix-adoption-route-2026-11-16.md`.
* **Creation**: Documented concept `decisions/comix-adoption-route-2026-11-16.md` (Comix.to adoption for cppIsekai: host needs (Route A lightpanda run evaluate)).
* **Creation**: Documented concept `decisions/weebcentral-port-2026-11-16.md` (Weeb Central ported as Lua plugin (plain-HTML, CF-safe)).
* **Update**: Linked `architecture/1manga-plugin-split.md` to `findings/lua-plugin-audit-2026-11-16.md` (1manga was split into module-per-purpose files and used as the healthy reference for the plugin fleet audit.).
* **Update**: Updated concept `findings/lua-plugin-audit-2026-11-16.md`.
* **Creation**: Documented concept `architecture/1manga-plugin-split.md` (1manga plugin source split into purpose modules).
* **Update**: Linked `findings/store-serialized-ops.md` to `architecture/store-orm-migration.md`.
* **Update**: Linked `architecture/store-orm-migration.md` to `findings/store-serialized-ops.md`.
* **Update**: Linked `decisions/api-cache-lock-miss-regression.md` to `findings/store-serialized-ops.md`.
* **Creation**: Documented concept `findings/store-serialized-ops.md` (database is locked: concurrent store ops on the one shared connection).
* **Update**: Linked `constraints/host-text-trim-preserves-casing.md` to `architecture/plugin-system.md`.
* **Update**: Linked `findings/1manga-site-quirks.md` to `references/plugin-porting-matrix-goIsekai.md`.
* **Creation**: Documented concept `constraints/host-text-trim-preserves-casing.md` (host.text.trim / strip_html preserve casing).
* **Creation**: Documented concept `findings/1manga-site-quirks.md` (1manga.co scraping quirks (HTML port)).
* **Update**: Linked `findings/plugins-reload-segfault.md` to `findings/sol2-lua-functor-teardown-corruption.md` (This was the surface symptom; the toolchain-rooted analysis lives there.).
* **Update**: Linked `findings/sol2-lua-functor-teardown-corruption.md` to `constraints/lua-host-tools-require-clang.md` (The finding's resolution is this constraint: the corruption only exists under GCC, and the project toolchain is clang.).
* **Creation**: Documented concept `constraints/lua-host-tools-require-clang.md` (Lua host tools require the clang toolchain).
* **Update**: Updated concept `findings/sol2-lua-functor-teardown-corruption.md`.
* **Update**: Linked `findings/sol2-lua-functor-teardown-corruption.md` to `decisions/luajit-migration.md` (LuaJIT was previously evaluated and skipped; it does not resolve this bug — the corruption is in sol2’s metatable dispatch, not in the Lua interpreter, so the Lua VM is not the lever.).
* **Update**: Linked `findings/sol2-lua-functor-teardown-corruption.md` to `constraints/sol2-lua-version.md` (Both are sol2 environment constraints discovered this session: Lua pinned to 5.4.8 (compat header rejects 5.5) + Linux xrepo recipe falls back to pacman::sol2, which silently resolved system headers until the system sol2 was removed this evening.).
* **Update**: Linked `findings/plugins-reload-segfault.md` to `findings/sol2-lua-functor-teardown-corruption.md` (Same crash; the corruption detail concept holds the ASan backtrace, ruled-out list, upstream links, and verified-fix candidates.).
* **Creation**: Documented concept `findings/sol2-lua-functor-teardown-corruption.md` (sol2 3.5.0 corrupts Lua functor teardown (plugins/reload segfault)).
* **Update**: Updated concept `findings/plugins-reload-segfault.md`.
* **Update**: Updated concept `findings/valid-title-pair-charset.md`.
* **Creation**: Documented concept `findings/lua-plugin-audit-2026-11-16.md` (Lua plugin end-to-end audit 2026-11-16 (search/detail/chapters/save)).
* **Creation**: Documented concept `findings/valid-title-pair-charset.md` (valid_title_pair rejects real-world manga slugs (apostrophes, exclamation marks)).
* **Creation**: Documented concept `findings/re2-findall-one-indexed.md` (host.re2.find_all is 1-indexed in Lua: m[1] = full match, groups start at m[2]).
* **Creation**: Documented concept `findings/plugins-reload-segfault.md` (/plugins/reload segfaults the cppIsekai daemon).
* **Update**: Updated concept `findings/mangasushi-cover-404-case-rename.md`.
* **Update**: Linked `findings/mangasushi-cover-404-case-rename.md` to `architecture/cover-prefetch-worker.md` (Broken covers surfaced through the /cache/img?cover=1 CoverCtx serving path; diagnosis of upstream 404s for stale lowercase URLs.).
* **Creation**: Documented concept `findings/mangasushi-cover-404-case-rename.md` (mangasushi: 2 covers 404, upstream case-renamed files, daemon emits stale lowercase URLs).
* **Creation**: Documented concept `decisions/drop-date-dependency.md` (Drop date (HowardHinnant) dependency; host_text dates via stdlib).
* **Creation**: Documented concept `findings/test-crashes-gdb-artifact.md` (Phantom test crashes were gdb-session artifacts; verify plain-run first).
* **Update**: Linked `decisions/openspec-crush-harness.md` to `references/tooling.md` (Tooling reference now also covers the OpenSpec harness dirs).
* **Creation**: Documented concept `decisions/openspec-crush-harness.md` (OpenSpec harness wiring: Crush gets .crush/ (local), universal copy stays in .agents/).
* **Update**: Linked `architecture/verified-layer-map.md` to `architecture/http-layer-gotchas.md` (Reconfirms the precompressed-asset trap; br_asset still does not negotiate Accept-Encoding).
* **Update**: Linked `architecture/verified-layer-map.md` to `references/code-layers.md` (Fresh 2026-11-16 survey; supersedes the stale plugin counts, /history route and pool sizing in this concept).
* **Creation**: Documented concept `architecture/verified-layer-map.md` (Verified layer map 2026-11-16 (runtimes, tables, cache v3, drift)).
* **Update**: Linked `references/site-quirks-mangafox.md` to `constraints/host-http-fetch-200-only.md`.
* **Update**: Linked `references/wasm-guest-pitfalls.md` to `constraints/host-http-fetch-200-only.md`.
* **Update**: Updated concept `decisions/plugin-cookie-jars.md`.
* **Creation**: Documented concept `constraints/host-http-fetch-200-only.md` (host.http.fetch* writes body only on HTTP 200 (wasm imports)).
* **Update**: Updated concept `references/wasm-guest-pitfalls.md`.
* **Update**: Updated concept `references/site-quirks-mangafox.md`.

## 2026-11-15
* **Creation**: Documented concept `decisions/renderer-fetch-engines.md` (Renderer fetch engines: lightpanda/obscura profiles in the fetch gate).
* **Creation**: Documented concept `constraints/host-chapter-state-keys.md` (Chapter state keys: one canonical (decoded) key space; rollup owns total_ch).
* **Creation**: Documented concept `constraints/plugin-status-canonical-vocab.md` (Plugin status strings: canonical vocab, normalized in-plugin).
* **Creation**: Documented concept `decisions/chapter-stats-total-cached-read.md` (Chapter rows: total/cached/read page counters).
* **Update**: Updated concept `decisions/guest-runtime-language.md`.
* **Update**: Updated concept `constraints/sol2-lua-version.md`.
* **Update**: Updated concept `decisions/third-party-dependencies.md`.
* **Update**: Linked `decisions/about-page-readme-single-source.md` to `decisions/static-assets-br.md` (About page renders markdown from the repo-root README.md using the same brotli-only vendored static asset serving decisions/static-assets-br defines (marked.min.js.br, mermaid.min.js.br via br_asset routes).).
* **Creation**: Documented concept `decisions/about-page-readme-single-source.md` (About page renders README.md live).
* **Update**: Linked `constraints/plugin-chapter-number-placeholders.md` to `decisions/chapter-zero-falsy-regression.md` (ch 0 is valid on mangadex; a plugin placeholder 0 must be told apart from a real prequel).
* **Creation**: Documented concept `constraints/plugin-chapter-number-placeholders.md` (Plugin chapter numbers are untrusted: order, duplicates and placeholder 0).
* **Creation**: Documented concept `constraints/test-binary-releasedbg-vs-release.md` (xmake test runs releasedbg; hand-run release test binaries go stale).
* **Update**: Updated concept `decisions/saved-manga-detail-source-of-truth.md`.
* **Update**: Linked `decisions/saved-manga-detail-source-of-truth.md` to `findings/detail-name-vs-library-card.md` (The 2026-11-15 empty-detail-name bug is what motivated storing resolved detail on saved_manga: the resolver ran but nothing durable held its output, so the next page open re-derived the name.).
* **Creation**: Documented concept `decisions/saved-manga-detail-source-of-truth.md` (Saved manga detail lives on saved_manga (planned, openspec manga-detail-table)).
* **Update**: Updated concept `findings/detail-name-vs-library-card.md`.
* **Creation**: Documented concept `findings/detail-name-vs-library-card.md` (Detail vs library title mismatch: apply_chosen trusts cached envelope with name:null).
* **Update**: Updated concept `architecture/reader-page-progress.md`.
* **Update**: Linked `decisions/ui-polish-grid-accessibility-contrast.md` to `architecture/status-flags-bitfield.md`.
* **Update**: Linked `decisions/ui-polish-grid-accessibility-contrast.md` to `architecture/per-manga-dim-blur.md`.
* **Update**: Linked `decisions/ui-polish-grid-accessibility-contrast.md` to `references/rec-ui-polish.md` (Implementation of U1, U3, U5, and dark-theme contrast fix from rec-ui-polish).
* **Creation**: Documented concept `decisions/ui-polish-grid-accessibility-contrast.md` (UI polish: category filters, card dim toggle, accessible names, and dark contrast fix).

## 2026-11-14
* **Update**: Linked `decisions/crash-traces-backward-cpp.md` to `decisions/xmake-build-system.md` (Crash-trace symbolization required build-system changes: the mode.releasedbg rule and the cxx_bin on_load ldflags hook in xmake.lua.).
* **Update**: Linked `findings/xmake-ldflags-on-load-hook.md` to `decisions/crash-traces-backward-cpp.md` (The on_load -rdynamic export documented here is what lets backward's dladdr resolver name crash-trace frames; without it the crash-trace decision degrades to bare addresses.).
* **Creation**: Documented concept `findings/xmake-ldflags-on-load-hook.md` (xmake cxx_bin: def-time add_ldflags can drop flags; on_load t:add is the reliable route).
* **Creation**: Documented concept `decisions/crash-traces-backward-cpp.md` (Crash traces via backward-cpp: fatal-signal handler in main, dynsym-only names, stderr capture).
* **Update**: Linked `decisions/cache-ns-display-name.md` to `architecture/cache-layout-v3.md` (the namespace replaces the per-plugin directory component described there, and drops its context-free sha fallback).
* **Update**: Updated concept `decisions/cache-ns-display-name.md`.
* **Update**: Linked `findings/rec-backlog-triage.md` to `references/rec-architecture-improvements.md` (triage of the backlog written in that reference).
* **Update**: Linked `decisions/cache-ns-display-name.md` to `architecture/cache-layout-v3.md` (the namespace replaces the per-plugin directory component described there).
* **Creation**: Documented concept `decisions/cache-ns-display-name.md` (Cache namespace = plugin display name, not plugin id).
* **Update**: Updated concept `references/rec-ui-polish.md`.
* **Update**: Updated concept `references/rec-architecture-improvements.md`.
* **Creation**: Documented concept `findings/rec-backlog-triage.md` (Rec backlog triage: 2 items already done, A7 proven bug, 3 OpenSpec changes opened).
* **Update**: Linked `references/rec-ui-polish.md` to `references/rec-architecture-improvements.md` (Architecture backlog; A3 and A5 gate parts of the UI work.).
* **Update**: Linked `references/rec-architecture-improvements.md` to `references/rec-ui-polish.md` (UI items U7 depends on architecture item A3.).
* **Creation**: Documented concept `references/rec-ui-polish.md` (Recommendation: UI polish backlog (for another model to pick up)).
* **Creation**: Documented concept `references/rec-architecture-improvements.md` (Recommendation: architecture improvements backlog (for another model to pick up)).
* **Update**: Linked `decisions/xrepo-round-2-deps.md` to `decisions/dummy-seed-removal.md` (Removal of dummy seed data.).
* **Update**: Linked `decisions/xmake-build-system.md` to `decisions/browser-smoke-tooling.md` (Smoke-test tooling used alongside the build.).
* **Update**: Linked `architecture/manga-viewer-core.md` to `decisions/source-layout-policy.md` (Source file layout policy.).
* **Update**: Linked `architecture/manga-detail-page.md` to `decisions/template-golden-test.md` (Golden render tests lock the template output.).
* **Update**: Linked `references/code-layers.md` to `architecture/settings-page-openspec.md` (Settings page change under the http/template layers.).
* **Update**: Linked `architecture/image-cache-offline.md` to `architecture/local-first-reads.md` (Local-first read path.).
* **Update**: Linked `decisions/plugin-runtimes-wasm-js.md` to `references/mangafire-vrf-algorithm.md` (VRF algorithm reference for a plugin using host crypto.).
* **Update**: Linked `architecture/plugin-system.md` to `references/host-crypto-vrf-sign.md` (Host crypto helper for VRF signing.).
* **Update**: Linked `architecture/manga-detail-page.md` to `decisions/restart-and-cbz-gate.md` (Restart and CBZ export gate.).
* **Update**: Linked `references/code-layers.md` to `decisions/db-read-parallel.md` (Parallel db reads through the store layer.).
* **Update**: Linked `architecture/image-cache-offline.md` to `architecture/cover-prefetch-worker.md` (Background cover prefetch worker.).
* **Update**: Linked `architecture/manga-viewer-core.md` to `references/goisekai-plugin-examples.md` (Example plugins from the predecessor project.).
* **Update**: Linked `decisions/deps-and-plugins.md` to `decisions/luajit-migration.md` (Lua runtime migration decision.).
* **Update**: Linked `decisions/plugin-runtimes-wasm-js.md` to `findings/js-image-referer-gap.md` (Image referer gap found in the js runtime.).
* **Update**: Linked `architecture/plugin-system.md` to `decisions/host-url-codec.md` (Host URL codec helper exposed to plugins.).
* **Update**: Linked `decisions/xmake-build-system.md` to `constraints/xmake-install-interrupt.md` (xmake package install interruption pitfall.).
* **Update**: Linked `decisions/xrepo-round-2-deps.md` to `constraints/utf8proc-2.11-3-api.md` (utf8proc 2.11.3 API constraint.).
* **Update**: Linked `architecture/manga-detail-page.md` to `decisions/chapter-zero-falsy-regression.md` (Chapter number zero regression affecting chapter rows.).
* **Update**: Linked `references/code-layers.md` to `decisions/search-cache-jsonb.md` (Search cache storage in the store layer.).
* **Update**: Linked `architecture/image-cache-offline.md` to `decisions/api-cache-null-payload.md` (Null-payload handling in the api cache.).
* **Update**: Linked `architecture/manga-viewer-core.md` to `decisions/plugin-website-field.md` (Plugin manifest website field.).
* **Update**: Linked `decisions/deps-and-plugins.md` to `decisions/curl-impersonate-chrome-only.md` (curl-first fetch: impersonation profile scope.).
* **Update**: Linked `decisions/plugin-runtimes-wasm-js.md` to `decisions/guest-runtime-language.md` (Guest runtime language choice.).
* **Update**: Linked `architecture/plugin-system.md` to `decisions/plugin-lifecycle.md` (Plugin load/unload lifecycle.).
* **Update**: Linked `decisions/xmake-build-system.md` to `decisions/build-toolchain-cpp23.md` (C++23 toolchain choice for the build.).
* **Update**: Linked `decisions/xrepo-round-2-deps.md` to `constraints/sol2-lua-version.md` (Lua version constraint behind the system lua pin.).
* **Update**: Linked `architecture/manga-detail-page.md` to `decisions/chapter-list-controls.md` (Read-state routes and chapter rows on the detail page.).
* **Update**: Linked `references/code-layers.md` to `decisions/static-assets-br.md` (Static asset serving in the http layer.).
* **Update**: Linked `architecture/image-cache-offline.md` to `decisions/api-cache-lock-miss-regression.md` (Local-first regression from db lock read as cache miss.).
* **Update**: Linked `architecture/manga-viewer-core.md` to `decisions/plugin-backend-interface.md` (Backend dispatch stays struct-based until a 4th runtime.).
* **Update**: Linked `decisions/plugin-runtimes-wasm-js.md` to `decisions/plugin-port-js-wasm.md` (Porting decision for the js/wasm runtimes.).
* **Update**: Linked `architecture/plugin-system.md` to `references/goisekai-abi-mapping.md` (ABI mapping reference from the predecessor project.).
* **Update**: Linked `decisions/xrepo-round-2-deps.md` to `constraints/xmake-re2-pin.md` (Constraint record for the re2 pin.).
* **Update**: Linked `references/code-layers.md` to `architecture/http-layer-gotchas.md` (Compile and runtime traps in the http layer.).
* **Update**: Linked `architecture/image-cache-offline.md` to `decisions/cache-stale-hit-guard.md` (Guard rules for serving stale cache hits.).
* **Update**: Linked `architecture/plugin-system.md` to `architecture/atsumaru-plugin.md` (Concrete site plugin built on this packaging/ABI.).
* **Update**: Linked `findings/library-status-flags-followups.md` to `architecture/status-flags-bitfield.md` (Followup gaps G1-G5 were resolved by this bitfield design.).
* **Update**: Linked `architecture/status-flags-bitfield.md` to `architecture/per-manga-dim-blur.md` (Dim state is status_flags bit6; the dim toggle writes through saved_dim_update on this bitfield.).
* **Update**: Updated concept `architecture/per-manga-dim-blur.md`.
* **Update**: Linked `findings/free-worker-routing.md` to `findings/status-flags-apply-migration-order.md` (The status-flags apply was delegated to the free worker; this finding records the delegation routing used for it.).
* **Update**: Linked `architecture/status-flags-bitfield.md` to `architecture/library-grid-stats.md` (The bitfield decision supersedes the earlier bit0/bit1-only status_flags contract described by library-grid-stats.).
* **Update**: Linked `findings/status-flags-apply-migration-order.md` to `architecture/status-flags-bitfield.md` (The apply finding is the migration consequence of the bitfield decision: dropping has_update required a backfill into status_flags first.).
* **Creation**: Documented concept `findings/free-worker-routing.md` (Delegating to a free model: native subagent skill beats spawn_agents (no model field)).
* **Update**: Updated concept `findings/library-status-flags-followups.md`.
* **Creation**: Documented concept `findings/status-flags-apply-migration-order.md` (status-flags apply: backfill must precede DROP COLUMN has_update (18/31 rows at risk)).
* **Creation**: Documented concept `architecture/status-flags-bitfield.md` (status_flags bitfield: bit0-6 layout + single compose_flags writer).
* **Creation**: Documented concept `findings/library-status-flags-followups.md` (status_flags: followup gaps left open by the detail-source-of-truth fix (c0fce4e)).
* **Update**: Updated concept `architecture/library-grid-stats.md`.
* **Creation**: Documented concept `references/site-quirks-mangafox.md` (MangaFox (fanfox.net) site quirks (wasm guest)).
* **Update**: Updated concept `references/wasm-guest-pitfalls.md`.
* **Creation**: Documented concept `decisions/guest-runtime-language.md` (Guest runtime language policy).

## 2026-11-13
* **Update**: Updated concept `findings/js-image-referer-gap.md`.
* **Update**: Linked `references/site-quirks-mangatown-mangafreak.md` to `references/wasm-guest-pitfalls.md`.
* **Update**: Linked `references/site-quirks-mangatown-mangafreak.md` to `references/plugin-porting-matrix-goIsekai.md`.
* **Update**: Linked `references/wasm-guest-pitfalls.md` to `references/plugin-porting-matrix-goIsekai.md`.
* **Creation**: Documented concept `references/site-quirks-mangatown-mangafreak.md` (Site quirks: mangatown + mangafreak (porting notes)).
* **Creation**: Documented concept `references/wasm-guest-pitfalls.md` (WASM guest authoring pitfalls (wasm3, from mangafreak pilot)).
* **Creation**: Documented concept `findings/js-image-referer-gap.md` (Host image fetch is plugin-blind: no Referer (js mangatown pilot finding)).
* **Update**: Linked `references/host-tool-gap-analysis.md` to `references/daemon-run-verify.md` (P1–P9 host tools verified live through the daemon restart documented there.).
* **Update**: Linked `references/host-tool-gap-analysis.md` to `references/openspec-delta-spec-format.md` (The delta format gate blocked this change's first archive attempt (2026-11-13).).
* **Creation**: Documented concept `references/daemon-run-verify.md` (Daemon run & verify (run-cppIsekai.sh)).
* **Creation**: Documented concept `references/openspec-delta-spec-format.md` (openspec delta spec format (archive gate)).
* **Update**: Updated concept `references/host-tool-gap-analysis.md`.
* **Update**: Linked `decisions/xrepo-packages-offline-workflow.md` to `decisions/xrepo-round-2-deps.md` (Round-2 pins are the package set exported by this offline workflow).
* **Update**: Linked `decisions/xrepo-packages-offline-workflow.md` to `decisions/xmake-build-system.md` (Same build-system decision family: xmake primary, with this concept covering offline package pinning/export around it).
* **Creation**: Documented concept `decisions/xrepo-packages-offline-workflow.md` (xrepo offline packages workflow: batch export + import -i packages (packages/ stays gitignored)).
* **Update**: Linked `references/host-tool-gap-analysis.md` to `references/plugin-porting-matrix-goIsekai.md` (Gap analysis menginformasikan prioritas porting plugin (P1-P9) — matrix menentukan site mana yang di-port, analysis menentukan apa yang perlu ditambal di host dulu.).
* **Creation**: Documented concept `references/host-tool-gap-analysis.md` (host-tool gap analysis (goIsekai → cppIsekai)).
* **Creation**: Documented concept `references/plugin-porting-matrix-goIsekai.md` (goIsekai -> cppIsekai plugin porting matrix).
* **Creation**: Documented concept `constraints/xmake-re2-pin.md` (re2 xrepo version pin).
* **Creation**: Documented concept `constraints/xmake-install-interrupt.md` (xmake pkg-install interrupted leaves empty hash-dirs).
* **Creation**: Documented concept `constraints/sol2-lua-version.md` (sol2 3.5.0 rejects Lua 5.5 from Arch pacman).
* **Creation**: Documented concept `constraints/utf8proc-2.11-3-api.md` (utf8proc 2.11.3 C API surface).
* **Creation**: Documented concept `decisions/xrepo-round-2-deps.md` (Second xrepo round: pkgconfig deps + utf8proc NFKD tokenizer).
* **Update**: Updated concept `decisions/third-party-dependencies.md`.

## 2026-11-12
* **Update**: Updated concept `decisions/third-party-dependencies.md`.
* **Creation**: Documented concept `decisions/plugin-lifecycle.md` (Plugin lifecycle: disabled = cache-only reads; folder-removal GC purges every trace).
* **Creation**: Documented concept `decisions/plugin-port-js-wasm.md` (Plugin port to JS and WASM — plan).
* **Creation**: Documented concept `decisions/plugin-backend-interface.md` (Plugin backends stay struct+dispatch; introduce PluginBackend interface only for a 4th runtime or out-of-process).
* **Creation**: Documented concept `references/host-crypto-vrf-sign.md` (host.crypto.vrf_sign: host-executed VRF signing (tables plugin-side)).
* **Creation**: Documented concept `references/mangafire-vrf-algorithm.md` (Mangafire VRF algorithm (port target for host.crypto.vrf_sign)).
* **Creation**: Documented concept `decisions/luajit-migration.md` (LuaJIT migration: skipped (not faster, higher risk)).
* **Update**: Updated concept `references/goisekai-abi-mapping.md`.
* **Creation**: Documented concept `references/goisekai-abi-mapping.md` (goIsekai to cppIsekai plugin ABI mapping).
* **Update**: Updated concept `decisions/xmake-build-system.md`.
* **Creation**: Documented concept `decisions/xmake-build-system.md` (xmake build system alongside cmake + prod-code gate tasks).
* **Update**: Updated concept `architecture/http-layer-gotchas.md`.
* **Update**: Updated concept `architecture/per-manga-dim-blur.md`.
* **Creation**: Documented concept `architecture/per-manga-dim-blur.md` (Per-manga dim+blur cover toggle).
* **Update**: Updated concept `architecture/atsumaru-plugin.md`.
* **Update**: Linked `architecture/library-dedup.md` to `architecture/library-grid-stats.md` (both land saved_manga columns and are synced from the same envelope envelope-derived sync pass).
* **Update**: Linked `architecture/library-dedup.md` to `architecture/library-page.md` (dup_group/alt_titles fuel the duplicate chip, dup sort and cross-plugin dedup UX on library-page).
* **Update**: Linked `architecture/library-grid-stats.md` to `architecture/library-page.md` (library-grid-stats stores and syncs the stat columns that library-page badges and filters render).
* **Update**: Updated concept `architecture/library-grid-stats.md`.
* **Update**: Updated concept `architecture/library-page.md`.
* **Update**: Updated concept `architecture/library-dedup.md`.

## 2026-11-11
* **Update**: Updated concept `architecture/library-dedup.md`.
* **Creation**: Documented concept `architecture/library-dedup.md` (library-dedup: signal columns on saved_manga; site plugins carry description, not synopsis).
* **Update**: Updated concept `architecture/library-page.md`.
* **Creation**: Documented concept `architecture/http-layer-gotchas.md` (src/http compile gotchas: global-scope helpers in api_ctx.hpp; crow::response is move-only).
* **Creation**: Documented concept `architecture/library-grid-stats.md` (library-grid-stats: saved_manga stat columns + envelope-derived sync).
* **Creation**: Documented concept `decisions/db-read-parallel.md` (DB read-parallel deferred: keep single-writer db_pool; read-only scans on demand).
* **Update**: Updated concept `decisions/chapter-list-controls.md`.
* **Update**: Updated concept `architecture/atsumaru-plugin.md`.
* **Update**: Updated concept `decisions/api-cache-lock-miss-regression.md`.
* **Update**: Linked `architecture/atsumaru-plugin.md` to `references/goisekai-plugin-examples.md` (Atsumaru was ported from the goIsekai reference corpus (example plugin + HAR)).
* **Update**: Linked `decisions/api-cache-lock-miss-regression.md` to `architecture/local-first-reads.md` (The db-lock-as-cache-miss bug violates the local-first read contract).
* **Creation**: Documented concept `decisions/api-cache-lock-miss-regression.md` (api_cache db-lock treated as cache miss: local-first regression (UNFIXED)).
* **Creation**: Documented concept `decisions/host-url-codec.md` (host.url_encode/url_decode: percent-codec host tool (no crypto yet)).
* **Creation**: Documented concept `decisions/plugin-website-field.md` (plugin manifest website field -> api rows + card link).
* **Creation**: Documented concept `architecture/atsumaru-plugin.md` (Atsumaru plugin (atsu.moe): Typesense search, CDN images, composite chapter ids).
* **Creation**: Documented concept `decisions/template-golden-test.md` (Golden-file render test for top-level templates).
* **Creation**: Documented concept `references/goisekai-plugin-examples.md` (goIsekai reference corpus: example plugins + HAR captures).
* **Update**: Updated concept `architecture/detail-page.md`.

## 2026-11-10
* **Creation**: Documented concept `decisions/static-assets-br.md` (Static assets: vendored, precompressed brotli-only via explicit routes).
* **Creation**: Documented concept `decisions/curl-impersonate-chrome-only.md` (curl-impersonate: chrome-family only; ini-fed profile dropdown; local-time crow log).
* **Creation**: Documented concept `decisions/restart-and-cbz-gate.md` (Settings restart button + per-chapter CBZ cache-full gate).
* **Creation**: Documented concept `architecture/local-first-reads.md` (Library reads are local-first).
* **Update**: Updated concept `decisions/source-layout-policy.md`.
* **Update**: Updated concept `decisions/chapter-zero-falsy-regression.md`.
* **Creation**: Documented concept `decisions/chapter-zero-falsy-regression.md` (reader: chapter 0 is valid — falsy-zero regression fixed + unit-locked).
* **Update**: Updated concept `architecture/settings-page-openspec.md`.
* **Creation**: Documented concept `architecture/settings-page-openspec.md` (settings-page: openspec change drafted (GET /config projection + /settings UI)).
* **Update**: Updated concept `architecture/cover-prefetch-worker.md`.
* **Creation**: Documented concept `architecture/cover-prefetch-worker.md` (cover prefetch: dedicated worker, cache-first, library + search hooks).
* **Creation**: Documented concept `decisions/api-cache-null-payload.md` (api_cache NULL payload served as 200-empty-body).
* **Creation**: Documented concept `decisions/dummy-seed-removal.md` (dummy seed plugins removed).
* **Creation**: Documented concept `decisions/search-cache-jsonb.md` (search-cache: api read model as JSONB in sqlite).
* **Creation**: Documented concept `decisions/browser-smoke-tooling.md` (browser smoke tooling: blueprint-mcp free mode).
* **Creation**: Documented concept `decisions/cache-stale-hit-guard.md` (cache stale-hit guard: wipe no longer serves 200-blank).
* **Creation**: Documented concept `decisions/chapter-list-controls.md` (chapter-list-controls: read-state routes and detail rows).
* **Creation**: Documented concept `architecture/reader-readahead.md` (reader: read-ahead cross-chapter prefetch (worker + 10-page window)).
* **Update**: Updated concept `references/reader-page-gaps.md`.
* **Creation**: Documented concept `architecture/reader-fullscreen.md` (reader-fullscreen: full-screen reader chrome + zoom persistence (implemented, uncommitted)).
* **Update**: Linked `architecture/reader-page-progress.md` to `references/reader-page-gaps.md` (reader-page decision references the gaps/facts backlog from the same session (zoom unpersisted, skip off-by-skip, legacy cn=0 rows, MangaDex empty at-home data[], main-branch commits).).
* **Creation**: Documented concept `references/reader-page-gaps.md` (reader-page: gaps and important facts (post-archive 2026-11-10)).
* **Update**: Updated concept `architecture/reader-page-progress.md`.

## 2026-11-09
* **Update**: Updated concept `architecture/reader-page-progress.md`.
* **Update**: Linked `architecture/reader-page-progress.md` to `architecture/cache-layout-v3.md` (reader-page pages land in the v3 layout img/<plugin>/manga/<slug>/chNNN-<id8>/pNNN.<ext>; the open /cache/img page-context 404 bug lives in img_ensure introduced by v3.).
* **Creation**: Documented concept `architecture/reader-page-progress.md` (reader-page 6/7: four-mode reader UI + persisted positions (4.1 live smoke pending)).
* **Update**: Linked `references/cache-v3-followup-backlog.md` to `architecture/cache-layout-v3.md` (Follow-up gaps discovered during cache-layout-v3 final review: cbz lexicographic sort, common/ cover namespace, VIPS link requirement).
* **Update**: Linked `architecture/cache-layout-v3.md` to `architecture/image-cache-offline.md` (Supersedes/extends: v3 reorganizes the v2 image pool into per-plugin layout and adds global transcode + host-computed cover ratio).
* **Creation**: Documented concept `references/cache-v3-followup-backlog.md` (cache-v3 follow-up backlog: cbz lexicographic sort, common/ cover namespace, VIPS link requirement).
* **Creation**: Documented concept `architecture/cache-layout-v3.md` (cache-layout-v3: per-plugin layout, global transcode, host cover ratio).
* **Update**: Linked `decisions/plugin-runtimes-wasm-js.md` to `architecture/plugin-system.md`.
* **Update**: Updated concept `decisions/plugin-runtimes-wasm-js.md`.
* **Update**: Updated concept `architecture/manga-detail-page.md`.
* **Update**: Linked `architecture/manga-detail-page.md` to `architecture/image-cache-offline.md`.
* **Update**: Linked `architecture/manga-detail-page.md` to `architecture/manga-viewer-core.md`.
* **Creation**: Documented concept `architecture/manga-detail-page.md` (manga-detail-page: stale-serve + fail-safe refresh pattern).

## 2026-11-08
* **Update**: Linked `references/uploads-cdn-tls-constraint.md` to `architecture/image-cache-offline.md`.
* **Update**: Linked `references/uploads-cdn-tls-constraint.md` to `decisions/fetch-profile-gate.md`.
* **Creation**: Documented concept `references/uploads-cdn-tls-constraint.md` (uploads CDN TLS constraint: system curl CLI is the only working engine).
* **Update**: Linked `decisions/source-layout-policy.md` to `architecture/manga-viewer-core.md` (layout policy for the host codebase this core serves).
* **Update**: Linked `decisions/source-layout-policy.md` to `references/code-layers.md` (supersedes the single-file crown descriptions in the layer map).
* **Update**: Updated concept `architecture/image-cache-offline.md`.

## 2026-11-07
* **Update**: Linked `decisions/deps-and-plugins.md` to `architecture/cli-config.md` (POST /config is a REST route per curl-first ruling; registry wraps CLI11/SimpleIni/glaze APIs.).
* **Update**: Linked `architecture/manga-viewer-core.md` to `architecture/cli-config.md` (Config layer feeds the host core (paths/port wiring to store, plugin, cache); registry is consumer of its dependency APIs.).
* **Creation**: Documented concept `architecture/cli-config.md` (cli-config: registry-backed config surface (CLI/env/ini/dump/save)).
* **Update**: Updated concept `architecture/image-cache-offline.md`.
* **Update**: Linked `decisions/third-party-dependencies.md` to `decisions/deps-and-plugins.md` (Extension of the vendoring policy: add libzippp+libzip (CBZ), stbrumme/hash-library, quickjs-ng; base64 via glaze; no hand-rolled crypto.).
* **Update**: Linked `architecture/plugin-system.md` to `decisions/deps-and-plugins.md` (Runtime parity: quickjs-ng becomes the second runtime with the same feature set & signatures (data-contract ABI intact; wasm3 not prioritized).).
* **Creation**: Documented concept `decisions/deps-and-plugins.md` (Dependencies & plugins: curl-first APIs, runtime parity, no hand-rolled crypto).
* **Update**: Linked `architecture/manga-viewer-core.md` to `architecture/image-cache-offline.md`.
* **Creation**: Documented concept `architecture/image-cache-offline.md` (cppIsekai image-cache + offline-first pathway).
* **Update**: Linked `references/mangadex-search-ranking.md` to `references/mangadex-auth-quirks.md` (Same verified-behaviors family: ranking quirk completes the auth quirks reference.).
* **Creation**: Documented concept `references/mangadex-search-ranking.md` (MangaDex search ranking = followedCount desc).
* **Update**: Linked `references/mangadex-auth-quirks.md` to `architecture/plugin-system.md` (MangaDex quirks constrained the mangadex plugin auth design and the storage host tools.).
* **Update**: Linked `decisions/plugin-dev-layout.md` to `decisions/plugin-data-segregation.md` (Dev layout realizes the data-segregation rule at the workflow level.).
* **Creation**: Documented concept `references/mangadex-auth-quirks.md` (MangaDex auth and API quirks (verified)).
* **Creation**: Documented concept `decisions/plugin-dev-layout.md` (Plugin dev layout: plugins_dev/<lang>/<site> + symlink deploy).
* **Update**: Linked `decisions/plugin-data-segregation.md` to `architecture/plugin-system.md` (MangaDex auth (config.json credentials + storage-backed tokens) is realized through plugin host tools defined here.).
* **Creation**: Documented concept `decisions/plugin-data-segregation.md` (Plugin data segregation: host .ini is host-only).
* **Update**: Linked `references/dependency-api.md` to `decisions/third-party-dependencies.md`.
* **Update**: Updated concept `references/dependency-api.md`.

## 2026-11-06
* **Update**: Linked `architecture/plugin-system.md` to `architecture/normalization-contract.md` (Wire-vs-storage clarification extends the unix-time + JSON policy (okf_update wipes relations)).
* **Update**: Updated concept `architecture/plugin-system.md`.
* **Update**: Linked `architecture/plugin-system.md` to `architecture/normalization-contract.md` (Plugins normalize to MangaDex canonical format and unix time per the contract).
* **Update**: Linked `architecture/plugin-system.md` to `architecture/manga-viewer-core.md` (Plugin system realizes the host-tools orchestration of the core architecture).
* **Creation**: Documented concept `architecture/plugin-system.md` (Plugin system: folder packaging, manifest, multi-runtime data-contract ABI).
* **Update**: Linked `architecture/normalization-contract.md` to `architecture/fts5-search-dedup.md` (The unix-time + JSON normalization policy sharpens the contract the plugins execute).
* **Update**: Linked `architecture/normalization-contract.md` to `architecture/detail-page.md` (Unix-time + JSON policy refines the normalisation contract concept (okf_update wipes relations)).
* **Update**: Updated concept `architecture/normalization-contract.md`.
* **Update**: Linked `architecture/normalization-contract.md` to `architecture/manga-viewer-core.md` (Normalisation duty belongs to plugins orchestrated by the host).
* **Update**: Linked `architecture/normalization-contract.md` to `architecture/detail-page.md` (Normalisation contract refines what the detail page displays and chapters record).
* **Creation**: Documented concept `architecture/normalization-contract.md` (Normalisation contract: MangaDex canonical + chapter release_at).
* **Update**: Linked `architecture/fts5-search-dedup.md` to `architecture/library-page.md` (FTS5 tables are part of the library data model).
* **Update**: Linked `architecture/fts5-search-dedup.md` to `references/code-layers.md` (FTS5 search upgrades the Store search layer described in code-layers).
* **Creation**: Documented concept `architecture/fts5-search-dedup.md` (Search & duplicate detection: FTS5 + BM25).
* **Update**: Linked `architecture/detail-page.md` to `architecture/manga-viewer-core.md` (Enrichment plugins are a second capability family of the plugin system).
* **Update**: Linked `architecture/detail-page.md` to `architecture/library-page.md` (Detail page extends the library title entity with metadata and enrichment).
* **Creation**: Documented concept `architecture/detail-page.md` (Detail page design + enrichment plugin (alt cover/alt info)).
* **Update**: Linked `architecture/library-page.md` to `architecture/manga-viewer-core.md` (Library page is a page of the core viewer architecture).
* **Update**: Linked `architecture/library-page.md` to `architecture/home-demo.md` (Library page design extends the home demo grid).
* **Creation**: Documented concept `architecture/library-page.md` (Library page design (status, progress, done/new badges)).
* **Update**: Linked `references/code-layers.md` to `architecture/home-demo.md` (Layer map shows how the home-demo decision is realized in source (paths updated after restructure)).
* **Update**: Updated concept `references/code-layers.md`.
* **Update**: Linked `references/tooling.md` to `references/code-layers.md` (Test/lint tooling executes the code-layer rules for future modules).
* **Creation**: Documented concept `references/tooling.md` (Test and lint tooling (doctest, clang-tidy, clang-format)).
* **Update**: Linked `references/dependency-api.md` to `decisions/third-party-dependencies.md` (Dependency decisions reference updated with the sqlite_orm entry).
* **Update**: Linked `architecture/store-orm-migration.md` to `references/dependency-api.md` (sqlite_orm entry in the dependency API reference with the escape/limit/pragma caveats).
* **Update**: Linked `architecture/store-orm-migration.md` to `references/code-layers.md` (ORM migration realizes the code-layer rules: Store remains the sole SQL gate, now via sqlite_orm).
* **Creation**: Documented concept `architecture/store-orm-migration.md` (Store migrated to sqlite_orm (Pimpl)).
* **Update**: Linked `references/dependency-api.md` to `decisions/third-party-dependencies.md` (API reference for the dependencies decided in this concept).
* **Update**: Linked `decisions/third-party-dependencies.md` to `decisions/build-toolchain-cpp23.md` (Dependency choices (system cli11, vendored simpleini, etc.) implement the toolchain/build decision).
* **Update**: Linked `references/code-layers.md` to `references/dependency-api.md` (Log bridge uses the verified ReaderWriterQueue/spdlog entry points).
* **Update**: Linked `references/code-layers.md` to `architecture/home-demo.md` (Code layer map shows how the home-demo decision is realized in source).
* **Creation**: Documented concept `references/code-layers.md` (Source layer map and rules (store / log / http)).
* **Update**: Updated concept `architecture/home-demo.md`.
* **Update**: Linked `architecture/home-demo.md` to `architecture/manga-viewer-core.md` (Home demo is the first slice of the core architecture).
* **Creation**: Documented concept `architecture/home-demo.md` (Home demo: library grid, plugin filter, live log).
* **Update**: Linked `architecture/manga-viewer-core.md` to `references/dependency-api.md` (Plugin ABI function names come from the verified dependency API reference).
* **Update**: Linked `architecture/manga-viewer-core.md` to `decisions/third-party-dependencies.md` (Core architecture consumes the vendored dependency set decided here).
* **Creation**: Documented concept `architecture/manga-viewer-core.md` (cppIsekai core architecture: plugin-per-site manga viewer).
* **Update**: Linked `references/dependency-api.md` to `decisions/third-party-dependencies.md` (API reference for the dependencies decided in this concept).
* **Update**: Linked `decisions/third-party-dependencies.md` to `decisions/build-toolchain-cpp23.md` (Dependency vendoring decision executes the toolchain decision).
* **Update**: Updated concept `references/dependency-api.md`.
* **Update**: Updated concept `decisions/third-party-dependencies.md`.
* **Update**: Updated concept `decisions/build-toolchain-cpp23.md`.
* **Update**: Linked `references/dependency-api.md` to `decisions/third-party-dependencies.md` (Function/API list of the dependencies decided in this concept).
* **Creation**: Documented concept `references/dependency-api.md` (cppIsekai dependency API surface).
* **Update**: Linked `decisions/third-party-dependencies.md` to `decisions/build-toolchain-cpp23.md` (Vendor/linking choices execute the toolchain decision (C++23, shared-first)).
* **Creation**: Documented concept `decisions/third-party-dependencies.md` (Dependency vendoring: system-shared first, minimal vendoring).
* **Creation**: Documented concept `decisions/build-toolchain-cpp23.md` (Build system: CMake + C++23, exe-only).
* **Creation**: Initialized OKF v0.2 project memory.
* **Creation**: Documented concept `decisions/plugin-runtimes-wasm-js.md` (Dual-runtime contract: wasm3 ABI + quickjs realm behind one shape/health core; parity tools; gas/interrupt budgets; JS-refcount, stack-rebase, glz-brace lessons).
* **Update**: Linked `decisions/plugin-runtimes-wasm-js.md` to `decisions/deps-and-plugins.md` and `architecture/plugin-system.md` (The execution contract implements both concepts' parity/vendoring plans).
* **Creation**: Documented concept `decisions/fetch-profile-gate.md` (Centralized impersonate gate in src/fetch/, per-call profile, live default; probed lib targets; firefox phase 2).
* **Update**: Linked `decisions/fetch-profile-gate.md` to `decisions/deps-and-plugins.md`, `architecture/cli-config.md`, `architecture/plugin-system.md` (The gate executes the curl-first ruling, uses the fetch_profile knob, extends the plugin opts ABI).
* **Creation**: Documented concept `decisions/plugin-cookie-jars.md` (Native libcurl cookie engine; per-plugin jar in data/plugin-state; mounted by the fetch gate; zero new dependencies).
* **Creation**: Documented concept `decisions/search-read-model-cache.md` (JSONB search read-model via opt-in call_cached; fresh=SELECT no HTTP; per-method TTL; file cache stays as fallback).
* **Update**: Linked `decisions/plugin-cookie-jars.md` to `decisions/plugin-data-segregation.md`, `decisions/fetch-profile-gate.md`, `architecture/plugin-system.md` (State-location policy, mounting gate, and the opts ABI).
* **Update**: Linked `decisions/search-read-model-cache.md` to `architecture/normalization-contract.md`, `architecture/plugin-system.md`, `references/code-layers.md` (jsonb decision source, wire-format principle, and the SQL layering).
* **Creation**: Documented concept `references/uploads-cdn-tls-constraint.md` (uploads.mangadex.org rejects both impersonate TLS stacks and a second in-process libcurl segfaults; image fetching shells out to the system curl CLI; covers load via /cache/img).
* **Update**: Linked `references/uploads-cdn-tls-constraint.md` to `decisions/fetch-profile-gate.md` and `architecture/image-cache-offline.md` (The constraint justifies the gate's plain/system path and the image fetcher's subprocess engine).
* **Creation**: Documented concept `decisions/source-layout-policy.md` (Post-split layout policy: crown facades, ~200-loc units, per-runtime seams, clang-doc doc-comments; sub-200 prune; audits in source-split-clangdoc/codeloc-final.txt).
* **Update**: Updated concept `decisions/fetch-profile-gate.md` (firefox pulled into phase 1: dual-.so dlopen vtable, family routing; user steer 2026-11-08).
* **Update**: Updated concept `decisions/fetch-profile-gate.md` (shipped state: real ff soname v0.6.1 pair vendored, probed safari15_3/_5 + ff target list, chrome-degrade path; change fetch-gate 6/6).
* **Creation**: Documented concept `decisions/weebcentral-port-2026-11-16.md` (weebcentral.com ported to Lua plugin, e2e verified 2026-11-16; HAR audit verdicts: todaymanga CF-502/unreachable, xbato timeout + CF challenge + image-decrypt JS, webnex live but RSC-streamed + 2 challenge-platform hits, not a clean CF-safe port).
