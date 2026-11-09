# Tasks

## 1. JSON endpoints (backend)

- [x] 1.1 `GET /api/detail/<plugin>/<slug>` + `GET /api/chapters/<plugin>/<slug>` via `call_cached` (stale-first, `cached:true`), slug guard (404 on separators/`..`); verify: curl against a live title returns envelope JSON, unknown slug 404s
- [x] 1.2 `POST /api/refresh/<plugin>/<slug>` per design D3 (call-through detail→chapters, overwrite cache, first-error-fails with cache intact); verify: curl refresh on a live title returns ok and the next detail fetch shows fresh data; failure path leaves cached data intact

## 2. Detail page (frontend)

- [x] 2.1 `templates/detail.mustache` per design D1 (layout + cover via `/cache/img`, Alpine fetch of metadata + chapter rows; chapter rows degrade field-by-field per design Risks); verify: page renders for a live title in a browser, chapters list visible
- [x] 2.2 Page route `GET /view/manga/<plugin>/<slug>` in `http_pages.cpp` + chapter-row actions: cbz button → existing `/export/cbz?plugin=&slug=<slug>/chNNN`, read link → existing `/reader/*` endpoints; verify: cbz download works from a row, read link opens the reader on a chapter

## 3. Verification

- [x] 3.1 Full suite green: `cmake --build build -j` zero new warnings, `ctest --test-dir build --output-on-failure` 10/10, `openspec validate --changes` passes; verify: commands run clean
- [x] 3.2 E2E smoke per spec scenarios: stale-serve (plugin-fail path via bogus upstream proxy) renders from cache with `cached:true`; refresh overwrite; unknown slug 404; verify: each scenario demonstrated live against the running daemon

## Workflow follow-up

- Archive the change once review requirements are satisfied.
- Verify the archived result.
