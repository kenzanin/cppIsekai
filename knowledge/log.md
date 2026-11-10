## 2026-11-10
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
