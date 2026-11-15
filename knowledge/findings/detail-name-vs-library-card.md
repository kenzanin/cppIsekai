---
type: finding
title: "Detail vs library title mismatch: apply_chosen trusts cached envelope with name:null"
description: "Library card title vs detail h1 title resolve differently. Frontend detail h1 reads detail.name, which apply_chosen (src/http/enrich_internal.hpp) fills ONLY when envelope_ok() is true. cached api_cache payloads store ok as SQLite integer 1 (from jsonb), envelope_ok requires glz bool -> false -> ensure_name never runs -> live /api/detail serves payload with operator-injected status_flags + empty name -> h1 shows '…'. 2026-11-15 uncommitted fix: ensure_name() resolver (chosen title -> scraper title -> saved_manga.name, slug-junk guard) plus html Cache-Control:no-cache and exit-to-detail moved top-right."
tags: [detail, library, apply_chosen, api-cache, envelope_ok, name, title, mangafreak, handoff]
generated: { by: agent/mcp, at: "2026-11-15T02:32:14Z" }
status: stable
---

## Symptom
Detail page (/view/manga/<plugin>/<slug>) h1 is empty (`…`) for mangafreak titles while the library card of the same manga shows a title. Chapter select on /read was also reported empty; live checks showed it actually renders (Alpine formatChapterLabel fixed in the same working-tree batch).

## Root cause (two independent issues)
1. enrich_internal.hpp apply_chosen(): `envelope_ok(env)` requires `ok` to be a glz bool (stale.hpp envelope_ok checks std::get_if<bool>). api_cache rows store the payload via `jsonb(?)` and read back via `json(payload)`; on this DB `json_extract(json(payload),'$.ok')` returns integer 1, not true, so a cached envelope fails envelope_ok and ensure_name() never executes. Fresh plugin envelopes may carry real true, so the bug is load-path dependent, not plugin dependent. This is why ANY plugin whose cached detail envelope was stored via jsonb shows an empty detail name until a real network refresh rewrites the cache.
2. Scraper envelopes carry `title`, never `name`. The detail page h1 reads detail.name. ensure_name() (added 2026-11-15) was meant to bridge this, but it is skipped because of (1).

## What landed where (2026-11-15, UNCOMMITTED working tree on branch ui-polish)
Files changed in tree (not committed):
- src/http/enrich_internal.hpp: ensure_name lambda in apply_chosen (resolves name = chosen title -> scraper data.title -> saved_manga.name, skipping saved name == slug). Signature of the fix is correct but does not run while envelope_ok rejects the payload. test_http_enrich make-envelope asserts `apply_chosen(...) == envelope` (no-row passthrough) and may fail once the fix is actually reached; re-lock expectations when fixing.
- src/http/http_pages.cpp: html() now returns `Cache-Control: no-cache` so broken reader shells are not pinned by the browser.
- templates/reader_top.mustache: exit-to-detail button moved to extreme top-right corner after the chapter select (right-facing exit icon, `hidden sm:inline` label).
- tests/fixtures/golden/reader.mustache.html: re-locked goldens (CPPISEKAI_UPDATE_GOLDEN=1 test_templates), pass 54/54.
Daemon :8090 (releasedbg) serves these patches; /health ok; live /api/detail/mangafreak/<slug> STILL returns "name":"" (see blocker).

## Saved_manga data repaired in-place (sqlite UPDATE, not migration)
- Dungeon_Tou_De_Yadoya... (mangafreak): name set from cached scraper title.
- 2 mangafreak rows (Isekai_Kaeri_No_Yuusha_Wa..., Shop_Skill_Saeareba...): NO cached title exists -> left as slug; they populate once detail refresh succeeds or enrichment runs.

## Verification status
- xmake -y ok (no incremental relink after header-only edit; binary mtime 09:16:46), test_templates pass, test_http_enrich 6/6 pass, /health ok.
- Chapter list claim NOT reproduced: reader inline select renders ch 63..1; detail grid renders ch 063..044; atsumaru APIs OK. Treat as Alpine-shell-vs-stale-browser issue fixed by the no-cache header + goldens.
- Live detail name STILL empty -> the bug is NOT closed. Verify with curl + sqlite json_extract commands below.

## Fix for next model
Pick one (1 preferred):
1. In stale.hpp envelope_ok (or the jsonb round-trip), coerce int 1 to true before the bool check. E.g. in call_cached after read_json: if `ok` node holds a double -> rewrite to bool via `static_cast<bool>(*num != 0.0)`; or normalize at api_cache_put so cached rows never need bool coercion. This fixes ALL cached-envelope consumers, not just detail.
2. Or make apply_chosen/title flow tolerate !envelope_ok payloads but preserve today's contract (rejected: spreads the fix and masks the storage bug).
Then: rebuild, restart daemon (releasedbg), curl /api/detail/mangafreak/<slug> -> expect name from data.title / saved_manga.name fallback; re-lock any golden/tests asserting passthrough. Commit the reader/title work separately from UI-polish (AGENTS: worktree hygiene).

## Open risks / notes
- api_cache rows are jsonb; ANY consumer asserting glz bool on cached envelopes is broken the same way (chapters pages related too). Grep `envelope_ok` and `get_if<bool>` across src/http/stale.hpp, src/http/enrich_internal.hpp, src/http/api_ctx.hpp, src/http/http_api_enrich.cpp, src/http/http_api_content.cpp before trusting any cached envelope.
- Chapter-list and title-empty symptoms were probably stale-shell/stale-envelope fallout, not plugin data bugs.
- Do NOT commit .cache/build artifacts; templates and golden stay together in one commit; knowledge/ notes commit separately (already done earlier for reader-page).
- codebase-memory confirmed exact qualified names: cppIsekai.src.http.http_pages.http.home_context (src/http/http_pages.cpp:25), cppIsekai.src.http.http_api_title.http::api.title_data_response (src/http/http_api_title.cpp:12-30). Library card title = home_context saved-mode loop: row[name] = ovl_title[i] (chosen_title -> title_meta) value_or m.name (saved_manga); detail h1 sources = literal detail.name from scraper envelope (apply_chosen). Two pipelines, so library shows, detail was blank.

## Evidence commands
- curl localhost:8090/api/detail/mangafreak/<slug> | tail -c 200
- sqlite3 data/library.db "SELECT json_type(payload,'$.ok'), json_extract(json(payload),'$.ok'), json_extract(json(payload),'$.data.ok') FROM api_cache WHERE plugin_id='mangafreak' LIMIT 1;"
- grep -n 'envelope_ok\|get_if<bool' src/http/stale.hpp src/http/enrich_internal.hpp src/http/api_ctx.hpp
