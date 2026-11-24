---
name: cppisekai-session-tooling
description: How to drive tools correctly in the cppIsekai repo — which tool to reach for (AFT search/zoom/inspect, codebase-memory graph, okf, shell, the browse browser extension, CDP probes), how to run and read xmake builds and tests, how to restart the daemon and verify it live, and the editing/JSON/SQL traps that have already burned this project. Use at the start of any session in cppIsekai, and whenever a build fails, a test is red, the daemon needs a restart, or you are about to grep/cat through shell.
---

# cppIsekai: correct tool usage

The goal is never "run a command". It is to get a trustworthy answer with the fewest tokens and the least chance of a false green. Every rule below exists because the opposite already cost this project a debugging detour.

## 1. Pick the tool by question type

| Question | Use | Not |
|---|---|---|
| Where is X / who calls X / blast radius | `aft_search`, `aft_callgraph` (callers, impact, trace_to, trace_data) | `grep -rn`, `find \| xargs grep` |
| Read one function | `aft_zoom(symbol=...)` | `read` whole file, `cat`, `sed -n` |
| File structure before reading | `aft_outline` | guessing line numbers |
| Architecture / "what exists in this subsystem" | `codebase-memory-mcp` (`search_graph`, `trace_path`, `get_architecture`) | reading the tree by hand |
| Project decisions, constraints, domain facts | `okf_search` first, `okf_show` only if needed | re-deriving them from code |
| Literal string, error message, config key | `aft_search(pattern=...)` or `grep` | semantic search |
| Git state, process state, file metadata, timings | `shell` | — |
| Is this page actually rendering? | `browse` (drives the user's real Chrome: snapshot/click/key/eval/screenshot) or a vendored-libcdp probe | trusting curl/API output |
| Did a DB write land? | `sqlite3 <db> "select ..."` | trusting the app's own log |

`aft_inspect` is a *hint*, not the compiler. Its `E`/`W` counters and clangd snippets can be stale or belong to a background refresh. A red panel text block is not proof of a build failure — the proof is `xmake`'s own exit status and its `error:` lines.

## 2. Shell discipline

- **Long commands get promoted to background at ~8 s.** `xmake -y` (~17 s) and `xmake test` (~23 s) always do. Either pass `wait: true` when the result is the next thing you need, or accept the background promotion and `bash_watch` once. Never `bash_status`-poll in a loop.
- **`bash_watch` sync max is 120 s.** A watch that times out is not a failure — watch again or end the turn and let the completion reminder wake you.
- **Keep output tiny.** Pipe through `grep -E "error:|passed|failed|build ok"`. Do not dump full build logs or full test logs into context; they drown the answer and invite misreads.
- **One test failure = run the binary yourself, not the suite.** `./build/linux/x86_64/release/test_store` prints the failing `FILE:LINE` with the exact `CHECK(...)` expression; the suite summary only names the target.
- **Quoting inside scripts.** `pkill -f <pattern>` inside a script self-matches the invoking `sh -c` and kills your own shell. Target pids parsed from `ss`, or exclude your own pid.
- **Tool names are exact.** `aft_search`, not `bash_search`/`grep_search`; if unsure, the §1 table is the canonical list. An invented name stalls the turn.
- **Identical call twice = you already have the answer.** If a tool result is byte-identical to the previous one, stop and act on it; repeated polling is the #1 turn-waster in this project's history.

## 3. Editing discipline

- **`edit` with `edits: [...]` is JSON.** A single lost escape turns the whole array into "edits must contain valid JSON" — the edit silently does nothing. If an edit returns that error, re-send it; do not assume it landed.
- **Never let a raw string lose its quote.** `R"(["A"])"` is the correct literal; `R"(["A"])` compiles as an unterminated macro and reports nonsense at a *later* line (`expected '}'`). When a brace/`}` error appears far from the edit, `od -c` the suspect line and compare against a known-good sibling line.
- **`import`/`#include` line rewrites** are rejected by the edit-tool validator (`#include` inside fenced content confuses it); use `sed -i` for those single lines, then rebuild.
- **`clang-tidy --fix --fix-errors` with the **default** check set corrupts this tree** (reversed subscripts, mangled if-init). It is safe only with a narrow explicit set + a build gate: `clang-tidy -p compile_commands.json --fix --fix-errors --checks='-*,performance-for-range-copy,performance-unnecessary-value-param,performance-inefficient-string-concatenation,modernize-use-emplace,modernize-use-nullptr,bugprone-use-after-move' --header-filter='^<abs>/src/' src/<dir>/*.cpp`, one folder at a time, `xmake -y` after each, revert the folder on red. Reject its one systematic false positive: `performance-unnecessary-value-param` on a value param that the body moves into a member (sink param) — revert those hunks. `--header-filter` anchored at the absolute `src/` path keeps third_party/vendor untouched.
- New files: `write`. Existing files: `edit`. A whole-file `write` over a live source file loses uncommitted work.

## 4. Build and test gates

```bash
xmake -y                              # full build; expect "build ok"; warning baseline is zero-warning -Wall -Wextra
xmake test                            # doctest suite, 21 targets
xmake test test_store                 # single target
./build/linux/x86_64/release/test_store   # when a target fails: exact CHECK lines
```

- The toolchain is clang. Never fall back to g++ to "get past" an error — that reintroduces the sol2 3.2 lambda-name collision that crashes plugin teardown.
- Template changes require regenerating goldens (`tests/test_templates.cpp`): run the regen, **review the diff**, then re-run the suite. A golden diff that is more than the intended lines means the template change leaked.
- Plugin changes require a daemon restart (Lua chunks compile per boot, no hot reload) *and* a cache purge of that plugin's `api_cache` rows — `call_cached` is local-first, so pre-fix rows shadow the fix indefinitely.
- Test DBs: give each new case a unique file name (`"test_store_x_" + run + ".db"`). A fixed name carries rows and schema across runs and produces phantom passes.
- **Sanitizers are opt-in and debug-only** (`--sanitize` in `xmake.lua`):
  ```bash
  xmake f -m debug --sanitize=address,undefined -y && xmake -y && xmake test   # leak+UB run
  xmake f -m debug --sanitize=thread -y && xmake -y                          # races (store/threads)
  xmake f -m release --sanitize=none -y && xmake -y                          # back to the daemon build
  ```
  In `release` the option is warn-and-skip on purpose: `build/linux/x86_64/release/cppIsekai` is the running daemon, and swapping it for an ASan binary is never what you want. ASan reports are exit-code failures, so `xmake test` reds on leaks — read `Direct leak` blocks first (our frames), and blame `Indirect leak` only after the direct rows are clean. Two real leaks came out of the first run (lexbor selector list ~20 KB per `html.select`, quickjs `JS_ToCStringLen`), so run this pass after touching any C-library boundary (lexbor/quickjs/lua/wasm3).
- `xmake scan` is the whole sweep in one command (ASan+UBSan suite, then TSan suite, then it restores the release build whether or not the sweep passed). Two full rebuilds, ~minutes — release-gate only, not per-commit.

## 5. Daemon: restart and prove it

```bash
./run-cppIsekai.sh                    # canonical stop/start; boots Chrome, binds :8090
setsid ./run-cppIsekai.sh >/tmp/d.log 2>&1 &   # detached, when a foreground stream would block
pgrep -af cppIsekai                   # confirm the path is build/linux/x86_64/release/...
curl -s -o /dev/null -w '%{http_code}' localhost:8090/
```

Facts that bite:

- `./run-cppIsekai.sh restart` ends in the server running **in the foreground**, so running it non-detached blocks until your tool timeout kills the process group — and the daemon dies with it. Use `./run-cppIsekai.sh stop; setsid ./run-cppIsekai.sh >/tmp/boot.log 2>&1 < /dev/null &`, then poll `curl` for 200 (cold boot ≈ 7 s, first boot with Chrome ≈ 50 s).
- A stale `./build/cppIsekai` (old CMake leftover) can be the process you are talking to. Check the path every time.
- Orphaned Chrome/Xvfb children inherit the listen fd, keep :8090 bound after the daemon dies, and make every rebind fail (crow logs it as the cosmetic `0.0.0.0:0`). `run-cppIsekai.sh` sweeps fd-holders; do not remove that sweep.
- `data/plugins/<id>` is a hardlink to `plugins_dev/lua/<site>` — edits at either location propagate, so "wrong plugin dir" is a dead hypothesis.
- Runtime plugin changes need a full restart; asset (`.js`, `.br`) changes need a **hard refresh** in the browser before reporting a visual symptom as fixed.

Verification ladder — climb it before saying "fixed":

1. Envelope/API: `curl` the plugin action or the `/api/...` route, read the exact field.
2. Route/image: `/cache/img` with full context (`cover=1`, or slug+chapter+page) — a missing param makes it 404 for unrelated reasons.
3. DB: `sqlite3 data/library.db` for the row you claim changed.
4. Browser: `browse` (§9) for anything the user described visually (cover blank, image identical, layout shifted). Server-side green does not refute a visual report.
5. Cache poisoning: if the user reports a symptom twice, hash the served bytes for two different URLs. Identical hashes mean the *input* is identical (bad referer, bad param truncation), not that the cache is broken.
- **Never judge a plugin by curling `/search?...`** — that route returns a JS shell; the result list arrives from `/plugin/<id>/search?...` in the browser. A `curl localhost:8090/search?plugin=X&q=Y` body containing "No results" proves nothing (it is the empty-state template). Probe `/plugin/<id>/search?q=...` and count `"id":` instead, then confirm visually in the browser if the user reported it.
- **Local DNS is ISP-poisoned on this machine** (Indonesian ISP `ioh.co.id`): a long list of domains resolve to a wildcard `*.ioh.co.id` cert, so libcurl reports `SSL peer certificate ... not OK` and Python says `Hostname mismatch` — for sites that are perfectly alive. Do not conclude "site down" from a local TLS/DNS failure. Check global truth with DoH (`https://dns.google/resolve?name=HOST&type=A`, `Status:3` = NXDOMAIN = really gone) and then confirm with a direct-IP TLS+GET using SNI. Only NXDOMAIN/SERVFAIL on a public resolver means the site is gone.

## 6. Store/SQLite specifics

- Every `Store` method opens with `const store_detail::OpTrace _op{"<name>"};`. Without it you bypass the recursive mutex around the single connection and get instant "database is locked".
- `api_cache.payload` is written through `jsonb(?)`. A payload that is not valid JSON makes the INSERT fail **silently** (fire-and-forget) and the row just is not there. Tests must pass `"{}"`, not `"P"`.
- Batch reads beat per-row calls on hot paths: `title_meta_get_many` (one OpTrace, one IN-per-plugin query) instead of 2N `title_meta_get`.
- Explicit transactions must be wrapped in `store_detail::TxGuard`, and reads must happen before `begin_transaction`.

## 7. Knowledge hygiene

- `codebase-memory-mcp/index_repository` after structural changes; `search_code` before editing unfamiliar code.
- `okf_search` before proposing architecture or dependencies. Never scan `knowledge/` with `ls`/`grep`/`find` — use `okf_*`.
- Durable facts learned the hard way go to `ctx_memory` (one fact per entry) or OKF; session-only knowledge is lost knowledge.
- Record decisions in OKF by **extending** the existing concept (`okf_update`) rather than creating a near-duplicate, then `okf_validate(strict=true)`.

## 8. Reporting

Report in Indonesian, casual, and lead with what is verified vs not. A green build is not a green feature. State the exact commands, exit statuses, and the field/row/pixel you observed; if a step is unverified, say so plainly instead of implying it passed.

## 9. Browser verification: `browse`

`browse` drives the user's own Chrome (real profile, logins, extensions). It replaces `blueprint-mcp`/`chrome-devtools-mcp`, which were removed from `opencode.json`. Use it for any symptom the user described visually, and for behaviour that only exists at runtime (keyboard handlers, spread pairing, scroll containers).

- **`navigate` returns the tab id — hold it, and close the tab when done.** Every later call needs `tab`; a stale id from an earlier turn answers `isn't a browse tab: browse didn't open it and the user didn't lend it` — list with `tabs {action:"list"}`, reopen with `navigate({url})`, never retry the dead id. Always finish with `tabs {action:"close", tab}`: the user does not want agent tabs left in their browser.
- **Code Mode has no timers.** `setTimeout`/`setInterval` are `ReferenceError` inside `execute`; sleep with `wait_for({tab, ms: 400})` and wait on real client state with `wait_for({tab, urlRegex: /page=7/})`, `{text}`, or `{selector, state}`. `wait_for` has no `pattern`/`regex` key — that fails with *"wait_for needs a condition: text, textGone, url, urlRegex, ref or selector"*.
- **`eval` sees only the DOM.** Page globals (`readerPage`, Alpine internals) are not reachable and raise `ReferenceError`. Assert on `document` instead: element counts, `naturalWidth > 0`, `location.search`, text content. Return one `JSON.stringify(...)` value.
- **Key names for `press`:** `ArrowDown`/`ArrowUp`/`ArrowLeft`/`ArrowRight`, `PageDown`, `PageUp`, `Space`, `Enter`, `Escape`, `End`, `Home`; the param is `key` (string or list), with `count` to repeat.
- **Client state is debounced.** After a keypress that triggers `history.replaceState` or a fetch, read the URL only after `wait_for({urlRegex})` — an immediate `eval` reports the previous value.

Full call shapes and the trap table: skill `browse-mcp`.

## 10. Asset pipeline (`/static/*.js`, `app.css`)

`/static/reader.js` and `/static/detail.js` go through `br_asset` (`src/http/http_pages.cpp:292`), which serves `static/<name>.js.br` whenever it exists (and it does, tracked in git, **minified**). Editing only the `.js` is invisible in the browser — the old brotli body keeps shipping and Alpine data shows no new fields. After any edit: `xmake js` (minifies via uglify-js/bunx + brotli q11 → `scripts/minbr.ts`; sources stay editable), then navigate (not just reload) and check the new state key exists.

`static/app.css` is compiled by the same `xmake js` task (Tailwind v4 over `static/input.css`, the templates and the JS `:class` strings) and served as `app.css.br` — never the Play CDN. After both, `xmake pack-assets` repacks `data/assets.zip`; the running daemon serves the zip, not the source files, so an unrepacked asset is an invisible edit.
