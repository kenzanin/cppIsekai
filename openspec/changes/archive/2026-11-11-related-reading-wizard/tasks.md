# Tasks

## 1. Enrichment plugins
- [x] 1.1 `plugins_dev/lua/enrichment-mangadex/main.lua`: cap `recommendations` at 10 (`while #scored > 10`).
- [x] 1.2 `plugins_dev/lua/enrichment-mangaupdates/main.lua`: cap `recommendations` at 10 (`while #recs > 10`).
- [x] 1.3 `plugins_dev/lua/enrichment-*`: ABI `enrich(kind=recommendations)` returns `{items:[{name,url,id}]}` — unchanged shape; verify both sources emit `id`.

## 2. Chosen persistence
- [x] 2.1 `src/http/enrich_internal.hpp` `normalize_chosen`: add `related_add` to whitelist; validate array of objects with non-empty `name` + `url` (`id` optional string); reject whole payload on any bad entry (400).
- [x] 2.2 `apply_chosen` / reset path: include `related_add` in the `Reset detail` clear set (title, synopsis, artist, author, tags_add, tags_hidden, related_add); `Reset cover` keeps it.

## 3. HTTP / merge
- [x] 3.1 No new routes; `enrichment.chosen` in `/api/detail` envelope already carries the new key verbatim (glaze passthrough).
- [x] 3.2 Verify `/api/enrichment/chosen` round-trips `related_add` (add + remove-all as key absence).

## 4. Detail page UI
- [x] 4.1 `detail_script.mustache`: `mergedRelated()` — MD-first dedupe over `{name,url,id}` from `/api/enrichment/<src>/recommendations` responses (cached in `cand`); `addAllRelated()` → POST chosen `{...chosen, related_add: merged}`; `removeAllRelated()` → POST chosen minus `related_add`.
- [x] 4.2 `detail.mustache`: related-readings segment in the Fetch-detail wizard (read-only pills with MD/MU badge, count, **Add all** / **Remove all**); no per-item buttons.
- [x] 4.3 Related section renders union(scraper related, chosen.related_add) deduped `url||id||name`; hidden when empty.
- [x] 4.4 `loadRelated()` honors `pluginCaps` gate for scraper part (existing) and always merges chosen.related_add from `/api/detail` response.
- [x] 4.5 Reset detail flow refreshes Related section (already reloads via `load()`).

## 5. Tests & verification
- [x] 5.1 `tests/test_http_enrich.cpp`: chosen round-trip with `related_add`; 400 on missing url/name; reset detail clears related_add; reset cover preserves it.
- [x] 5.2 Golden templates re-baked (`CPPISEKAI_UPDATE_GOLDEN=1 ctest -R templates`).
- [x] 5.3 `ctest` all green; zero warnings on build.
- [x] 5.4 Live smoke (browser, atsumaru H6P2Z): fetch detail → Add all → Related shows 5 entries → reload persists → Remove all → hidden; network log: zero `/api/related/atsumaru/...` requests (atsumaru has no `related` cap; scraper call correctly gated off).
- [x] 5.5 `openspec validate related-reading-wizard --strict` passes (valid: true).
