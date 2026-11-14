# Tasks: cache-ns-display-name

## 1. Namespace helper + cache injection

- [x] 1.1 Add `cache::cache_ns(plugin_id, name)` in `src/cache/slug.hpp`: slugified display
      name, id when the name is blank or slugifies to nothing
- [x] 1.2 Add `Cache::NsResolver` + `Cache::ns(plugin)` in `src/cache/cache.hpp`, third
      `Cache` constructor argument defaulting to empty (id as namespace)
      - verified: `src/cache/cache.hpp` (`using NsResolver`, `[[nodiscard]] std::string ns(...) const`),
        `src/cache/cache.cpp` (`Cache::Cache(Config, Fetcher, NsResolver)`).

## 2. Apply the namespace at every path site

- [x] 2.1 `src/cache/cache_img.cpp`: page rel path, cover rel path, extension-free hit lookup
      and `cover_serve` tail all use `ns(...)`
- [x] 2.2 `src/http/http_api_content.cpp`: `chapter_cached` takes the namespace;
      `/api/cbz-ready` and `/export/cbz` pass `images.ns(plugin)`
- [x] 2.3 `src/main.cpp`: build `plugin_id -> cache_ns` from `store.plugins()` at boot and move
      it into the resolver

## 3. Cutover

- [x] 3.1 On the first boot with a resolver, rename `img/` to `.legacy-img` (swept on the next
      boot) and recreate `img/`, stamped by `.cache-ns-name`
      - verified: `src/cache/cache.cpp`; guarded on `ns_` so id-keyed pools booted without a
        resolver (all cache tests) are left alone — that guard is what keeps
        `tests/test_cache.cpp`'s pre-seeded `img/` fixtures honest.

## 4. Verification

- [x] 4.1 `tests/test_slug.cpp`: display name wins, id is the fallback (blank, whitespace,
      unsluggable), different ids with the same name collide, fs-safe and length-capped
- [x] 4.2 `tests/test_cache.cpp`: resolver keys the on-disk directory, empty resolver result
      falls back to the id, real `img/...` path asserted for both
- [x] 4.3 `xmake test` — 18/18 binaries green (test_cache 13/13, test_slug 6/6)
- [ ] 4.4 Live smoke on the daemon: confirm `data/cache/img/<name-slug>/` is created for a
      plugin whose display name differs from its id, and that the old `img/<id>/` tree is gone
      after two boots

## 5. Delete the context-free sha fallback

- [x] 5.1 `Cache::img_store` stores nothing without a placement path; `Cache::img_ensure`
      refuses before fetching; `path_for` yields "" for the img pool and `ensure`/`store`
      reject it — the `api/<sha>.json` pool keeps its sha naming
- [x] 5.2 `reader/worker.hpp`: `rel_path` legacy helper deleted, the loop drops a batch whose
      nudge has no plugin/slug/chapter
- [x] 5.3 Both nudge sites pass `WindowCtx`: `/chapter/<id>/skip` takes it from
      `position_last()`, the reader route skips the prefetch when the chapter number is unknown
- [x] 5.4 Tests: `test_cache` 13/13, `test_http_img` 6/6, `test_worker` 6/6 — the old
      sha-landing locks now assert the opposite (refused, nothing fetched, nothing written)
