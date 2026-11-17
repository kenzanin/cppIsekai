---
type: decision
title: "Comix.to adoption for cppIsekai: external lightpanda daemon + hand-rolled JSON-RPC (supersedes Route A recommendation)"
description: "SUPERSEDED 2026-11-17 by the app-local headless-Chrome backend (commit 9b0f374): lightpanda MCP daemon retired; same host.browser.call contract, vendored cdp-cpp CDP client, Chrome solves comix WAF+pager natively. Kept for the host-gap analysis history."
tags: [plugin, comix, lightpanda, browser, host-api]
generated: { by: agent/mcp, at: "2026-11-17T11:08:47Z" }
status: deprecated
---

Study of adopting goIsekai's examples/plugins/lua/comix into cppIsekai. goIsekai's comix is architected around a headless-browser host API (host.browser.fetch / host.browser.evaluate_with_init via CDP+rod). cppIsekai has no browser primitive, so the port must answer: what must the host add? All findings below are empirical (probes run 2026-11-16 against the live daemon and the lightpanda CLI), not inference. IMAGE ROUTE DO NOT BYPASS: comix search/detail/chapters/genres work without any new host code.

goIsekai comix host surface: json.encode/decode, text.url_encode/date_to_iso/trim/html_decode, regex.find(_all) (= cppIsekai re2), plugin.param, http.get_body, browser.fetch (3 uses), browser.evaluate_with_init (2 uses). Everything else is a rename/shape adaptation. Only the browser.* pair is missing.
cppIsekai equivalences already present:
- host.browser.fetch ~ fetch profile "lightpanda" via host.http.fetch(url, {profile="lightpanda", timeout_ms=...}) -> fetch_sync -> src/fetch/fetch.cpp rendered_fetch() (popen "lightpanda fetch <url> --dump html --wait-ms 8000"), the same plumbing used since 4ucomics. CLOBBERING: rendered_wait-ms is hardcoded 8000, not from timeout_ms (1-LOC fix).
- No way today to run JS in the page and get a value back: host.http.fetch returns response bytes, not an evaluated result.

EMPIRICAL EVIDENCE (live):
- lightpanda fetch 'https://comix.to/browse?q=isekai' --dump html renders 28 .lrow__poster cards (SSR/initial markup + client JS both fine).
- Chapter list renders server-side enough to scrape: mchap-list/mchap-item/mchap-row anchor hrefs + Chapter rows (verified 2026-11-13 dates) are inside the detail-page lightpanda dump; cppIsekai comix chapters endpoint already returns ISO created_at list (end-to-end live, e.g. Ch.22.5 2026-11-13).
- Reader page (chapter URL) SSR payload (#initial-data) contains only {mangaId:66777, mangaHid:"n7dg5", chapterId:11727328, chapterNumber:22.5} — the reader does NOT embed a page list.
- Reader slots render but images do not: dump shows .rpage-page data-page=1..6 + counter "1 / 6" but ZERO <img> tags. Confirms goIsekai's note: preload=all still defers until viewport touch; IntersectionObserver never fires in a scroll-less headless dump. Plain lightpanda fetch CANNOT obtain page image URLs (nor can injecting preload=all via --inject-script-file; evaluated probe still showed 0 img in dump html).
- lightpanda run <script.js> DOES work and is proven for the fetch-without-a-library path: global Page; string-expression evaluates; scroll harvest script (wait for .rpage-page -> scrollIntoView each unloaded slot -> collect img[src]) returned DETERMINISTIC 6/6 URLs (verified 2x runs), e.g. https://lov.<random-cdn-domain>/hi/...
- CDN serves with NO Referer (200 image/webp ~1.2MB) and 403 with ANY Referer — matches goIsekai's note.
- lightpanda mcp --port <p> --host 127.0.0.1 serves tool/JSON-RPC over plain HTTP POST /mcp (no WebSocket); initialize returned serverInfo {lightpanda 0.1.0}, capabilities tools — tool surface mirrors the rod path (goto/execEvaluate/waitForScript/click/fill).
- Old page URL the user reported (n7dg5-isekai-battle-royale:3689278-chapter-6) is delisted upstream — the 404 is expected; live URL is 11727328-chapter-22.5.

FINDINGS (what the host must add):
1. NEW primitive host.browser.evaluate(url, js) -> string: popen-based rendered_eval() sibling of rendered_fetch(); pipe the script via stdin (lightpanda run -), capture stdout; return raw stdout (plugins print JSON via console.log themselves). Budget via --terminate-ms; the scroll-harvester script ships inside the comix plugin, not the host. Lua binding in src/plugin/lua/lua_runtime.cpp; mirror in js_bindings.cpp/wasm_imports.cpp for runtime parity if other runtimes need it. Adding this ONE primitive unblocks pages (goIsekai's evaluate_with_init recipe verbatim).
2. Sentinel for "no Referer": manifest image_referer must be able to express "send NO Referer" for reader CDNs that 403 on any Referer. Today Host::image_referer() falls back to manifest.website origin when image_referer is empty -> comix gets Referer: https://comix.to -> 403 on every page image; non-empty becomes the header verbatim. Add "image_referer": "none" -> HTP curl invoked WITHOUT --referer (skip -e entirely). ~5 LOC in src/http/http.cpp (makeCurlFetcher) + src/plugin/plugin_core.cpp (image_referer). Files also: image_engine/sentinel handling for completeness.
3. OPTIONAL: pass timeout_ms through to rendered_fetch's --wait-ms (today 8000 hardcoded).
4. Explicitly NOT needed: CDP/WebSocket client, a new dependency, chrome binary wiring, decrypting the apicore crypto (XB double-SHA256 IV + Zj/_j RC4-ish material tables; never produced plaintext across two sessions and `w=f001f101` token rotates silently). Route via lightpanda is complete: images come from the page's own DOM after evaluate-harvest; strings-level decrypt is now an anti-goal.
5. Route C (scale-out): the SAME Lua contract host.browser.evaluate(url, script) can later be reimplemented over lightpanda mcp HTTP sessions (goto/execEvaluate/waitForSelector tools) if/when a second plugin needs persistent browser sessions (login, CF cookies, multi-step). No plugin changes needed. Transport migration stops being a plugin concern the day A ships behind this contract.

ROUTE OPTIONS vs CURRENT TREE (all probed, none speculative):
- Route A (popen evaluate, ~1-2h): RECOMMENDED NOW. Mirrors the 4ucomics popen precedent; zero deps; works today (6/6 harvest verified).
- Route C (lightpanda mcp over HTTP persist): DEFER until a second plugin needs browser sessions; slightly more glue (tool discovery, Mcp-Session-Id handling, session lifecycle).
- Route R2 (C++ CDP+WS client + Chrome): REJECT for now — Chrome is installed and proven only through the same scripts; lightpanda handles comix fully (verified). Revisit only if a target breaks lightpanda.

LIVE comix STATE (2026-11-16, unmodified): detail OK, chapters OK (ISO created_at), genres OK, search returns {"ok":true,"data":{"total":0,"items":{}}} — unverified root cause (either card-regex drift vs CURRENT browse markup aria-label/href order or v= cache-buster absent -> stale api_cache rows; NOT proven); pages FAIL with the exact class of error reported by the user (empty reader -> client-only page data).

NEXT: user to pick Route A vs C. If A: implement rendered_eval + image_referer sentinel + comix plugin pages harvest + diagnose search (live logs/api_cache), e2e verify (6/6 images via /cache/img), then commit. Blocked only on this choice.
