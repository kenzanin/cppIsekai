# Tasks: cache-layout

## 1. Storage layout v2
- [x] 1.1 `src/cache/slug.hpp`: fs-safe slug (lowercase, <=64 chars), collision suffix = last 8 chars of manga id, no DB; unit tests (empty title, unicode, collision, length cap)  <!-- bc8eaea -->
- [x] 1.2 `Cache` API gains optional `rel_path` on `ensure/store/peek` (shard key stays sha-of-URL; on-disk path switches); boot walk of root/ recursive + purge `.legacy-img-*`/`-api-*` after one boot; ctest green (`-R cache`), full suite green  <!-- 840a3a6 -->
- [x] 1.3 Fixtures in `tests/test_cache.cpp` exercising: eviction across tree depth, purge-once path with legacy `.legacy-img-*` detection  <!-- included in 840a3a6 -->

## 2. Reader worker placement
- [ ] 2.1 `src/reader/worker.hpp`: page image writes use `<plugin>/<slug>/chNNN/pNNN.<ext>` when rel_path known; sha fallback untouched; ctest green; live smoke optional flag in task 4
- [ ] 2.2 `/reader/window` unchanged inline — verify worker probe test writes to structured dir when slug present

## 3. CBZ export
- [ ] 3.1 vendor libzippp header pair under `third_party/`; CMake target find_package libzip + libzippp include; fallback to vendored minizip if linkage stalls (ponytail note in design D6)
- [ ] 3.2 `GET /export/cbz?plugin=<id>&slug=<slug>` stream entries (cover.<ext> + chNNN/pNNN.<ext>) in lexicographic order from disk, empty slug = empty zip; unit + live smoke route probing via binary + curl in next task

## 4. Verification
- [ ] 4.1 ctest full suite green; `openspec validate`
- [ ] 4.2 smoke: boot with pre-populated legacy `img/` folder → purge message in log + fresh structured writes; export slug whose pages exist → downloadable zip; window probe writes land structured
