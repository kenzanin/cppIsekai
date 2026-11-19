---
name: cppisekai-session-tooling
description: How to drive tools correctly in the cppIsekai repo — which tool to reach for (AFT search/zoom/inspect, codebase-memory graph, okf, shell, CDP probes, blueprint-mcp), how to run and read xmake builds and tests, how to restart the daemon and verify it live, and the editing/JSON/SQL traps that have already burned this project. Use at the start of any session in cppIsekai, and whenever a build fails, a test is red, the daemon needs a restart, or you are about to grep/cat through shell.
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
| Is this page actually rendering? | CDP probe (vendored libcdp) or `blueprint-mcp` against the real browser | trusting curl/API output |
| Did a DB write land? | `sqlite3 <db> "select ..."` | trusting the app's own log |

`aft_inspect` is a *hint*, not the compiler. Its `E`/`W` counters and clangd snippets can be stale or belong to a background refresh. A red panel text block is not proof of a build failure — the proof is `xmake`'s own exit status and its `error:` lines.

## 2. Shell discipline

- **Long commands get promoted to background at ~8 s.** `xmake -y` (~17 s) and `xmake test` (~23 s) always do. Either pass `wait: true` when the result is the next thing you need, or accept the background promotion and `bash_watch` once. Never `bash_status`-poll in a loop.
- **`bash_watch` sync max is 120 s.** A watch that times out is not a failure — watch again or end the turn and let the completion reminder wake you.
- **Keep output tiny.** Pipe through `grep -E "error:|passed|failed|build ok"`. Do not dump full build logs or full test logs into context; they drown the answer and invite misreads.
- **One test failure = run the binary yourself, not the suite.** `./build/linux/x86_64/release/test_store` prints the failing `FILE:LINE` with the exact `CHECK(...)` expression; the suite summary only names the target.
- **Quoting inside scripts.** `pkill -f <pattern>` inside a script self-matches the invoking `sh -c` and kills your own shell. Target pids parsed from `ss`, or exclude your own pid.

## 3. Editing discipline

- **`edit` with `edits: [...]` is JSON.** A single lost escape turns the whole array into "edits must contain valid JSON" — the edit silently does nothing. If an edit returns that error, re-send it; do not assume it landed.
- **Never let a raw string lose its quote.** `R"(["A"])"` is the correct literal; `R"(["A"])` compiles as an unterminated macro and reports nonsense at a *later* line (`expected '}'`). When a brace/`}` error appears far from the edit, `od -c` the suspect line and compare against a known-good sibling line.
- **`import`/`#include` line rewrites** are rejected by the edit-tool validator (`#include` inside fenced content confuses it); use `sed -i` for those single lines, then rebuild.
- **`clang-tidy --fix --fix-errors` corrupts this tree** (reversed subscripts, mangled if-init). Never run it; fix the reported site by hand.
- New files: `write`. Existing files: `edit`. A whole-file `write` over a live source file loses uncommitted work.

## 4. Build and test gates

```bash
xmake -y                              # full build; expect "build ok"; warning baseline is zero-warning -Wall -Wextra
xmake test                            # doctest suite, 19 targets
xmake test test_store                 # single target
./build/linux/x86_64/release/test_store   # when a target fails: exact CHECK lines
```

- The toolchain is clang. Never fall back to g++ to "get past" an error — that reintroduces the sol2 3.2 lambda-name collision that crashes plugin teardown.
- Template changes require regenerating goldens (`tests/test_templates.cpp`): run the regen, **review the diff**, then re-run the suite. A golden diff that is more than the intended lines means the template change leaked.
- Plugin changes require a daemon restart (Lua chunks compile per boot, no hot reload) *and* a cache purge of that plugin's `api_cache` rows — `call_cached` is local-first, so pre-fix rows shadow the fix indefinitely.
- Test DBs: give each new case a unique file name (`"test_store_x_" + run + ".db"`). A fixed name carries rows and schema across runs and produces phantom passes.

## 5. Daemon: restart and prove it

```bash
./run-cppIsekai.sh                    # canonical stop/start; boots Chrome, binds :8090
setsid ./run-cppIsekai.sh >/tmp/d.log 2>&1 &   # detached, when a foreground stream would block
pgrep -af cppIsekai                   # confirm the path is build/linux/x86_64/release/...
curl -s -o /dev/null -w '%{http_code}' localhost:8090/
```

Facts that bite:

- A stale `./build/cppIsekai` (old CMake leftover) can be the process you are talking to. Check the path every time.
- Orphaned Chrome/Xvfb children inherit the listen fd, keep :8090 bound after the daemon dies, and make every rebind fail (crow logs it as the cosmetic `0.0.0.0:0`). `run-cppIsekai.sh` sweeps fd-holders; do not remove that sweep.
- `data/plugins/<id>` is a hardlink to `plugins_dev/lua/<site>` — edits at either location propagate, so "wrong plugin dir" is a dead hypothesis.
- Runtime plugin changes need a full restart; asset (`.js`, `.br`) changes need a **hard refresh** in the browser before reporting a visual symptom as fixed.

Verification ladder — climb it before saying "fixed":

1. Envelope/API: `curl` the plugin action or the `/api/...` route, read the exact field.
2. Route/image: `/cache/img` with full context (`cover=1`, or slug+chapter+page) — a missing param makes it 404 for unrelated reasons.
3. DB: `sqlite3 data/library.db` for the row you claim changed.
4. Browser: CDP probe or `blueprint-mcp` for anything the user described visually (cover blank, image identical, layout shifted). Server-side green does not refute a visual report.
5. Cache poisoning: if the user reports a symptom twice, hash the served bytes for two different URLs. Identical hashes mean the *input* is identical (bad referer, bad param truncation), not that the cache is broken.

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
