# AGENTS.md - DMAA Protocol v0.1 - Instructions for AI Agents in `cppIsekai`

> Powered by [OKF Agent Memory](https://github.com/okf-memory/okf-agent-memory) -- Open Knowledge Format (OKF) v0.2 persistent project memory for AI agents.

Welcome to the **cppIsekai** repository. Define domain-specific behavioral rules in Agent Action Grammar (AAG) alongside the persistent memory protocol:

## 0. Project & Domain Codex (Customize for your domain: Research, Enterprise, Engineering, Creative)
- TONE: style == direct_concise, zero_pleasantries
- LANGUAGE: lang == en_US UNLESS channel == user_chat
- FORMAT: diagrams => ASSERT(syntax == mermaid, ELSE=STOP("All diagrams MUST use Mermaid syntax; ASCII/text art prohibited."))
- GOAL: maintain(high_factual_integrity, domain_conventions)

<!-- BEGIN OKF AGENT MEMORY -->
## 1. Behavioral Invariants & Constraints (RFC 2119)
- MUST execute `okf_search(query=keywords, limit=3)` before proposing architecture, new dependencies, or substantial changes.
- NEVER scan `knowledge/` via `list_dir`, `grep_search`, `find`, or raw file readers.
- NEVER forge human verification (`verified:` is human-only; declare `generated: { by: "<actor>", at: "<iso-time>" }`).
- PREFER native `okf_*` MCP tools OVER CLI fallback commands.
- CODEGRAPH: MUST keep the index fresh -- run `codebase-memory-mcp/index_repository` after structural code changes; MUST explore unfamiliar code via `codebase-memory-mcp/search_code` BEFORE editing it.
- OKF PERSISTENCE: MUST persist durable decisions/facts via `okf_create`/`okf_update` in the same session they are made -- session-only knowledge is lost knowledge.

## 2. Guard Clauses & Scope Governance
- ON edit(@path/):
    IF first_visit(@path/) => okf_search(for_path=@path/)
    IF governance == "hold" => STOP("Subsystem frozen by governance. Request explicit human confirmation.")
    IF governance == "constraint" => MUST adhere to all listed invariants
    IF governance == "context" => proceed with awareness
- ON user_query(architecture | requirements | conventions | domain_facts):
    okf_search(query=keywords, limit=3) => evaluate summary description
    IF relevant => okf_show(concept_id) ONLY on demand
    IF updating_existing_concept => PREFER okf_update OVER okf_create

## 3. Completion Pipeline (Sequential Assertion Gates)
1. IF decisions_made => MUST okf_create(category/*, type="decision", title=..., desc=...)
2. IF requirements_discovered => MUST okf_update(concept_id)
3. IF concepts_mutated => MUST sync(knowledge/log.md, knowledge/index.md)
4. ASSERT(okf_validate(strict=true, drift=true) == {errors: 0, warnings: 0}, ELSE=fix_before_exit)
<!-- END OKF AGENT MEMORY -->

---

## Project

cppIsekai — local-first C++23 manga viewer daemon (Linux, Arch). Crow master + crow+htmx templates, sqlite_orm store, plugin-per-site runtime (Lua via sol2 first; quickjs-ng planned as the second runtime per `decisions/deps-and-plugins`). Entry point: `src/main.cpp` — `build/cppIsekai` boots the daemon. Config surface is the registry in `src/config/` (CLI11 flags + `CPPISEKAI_*` env + ini layers; `--config` required, `--dump-config`, `POST /config`). Plugin ABI is JSON-text-in/out with a single host wrap point (`src/http/http.cpp`); host tools are `http.fetch`, `storage.*`, `log.*`.

## Commands

```bash
xmake -y                            # configure + build (xrepo packages auto-install; zero-warning gate -Wall -Wextra)
xmake test                          # full suite (doctest)
xmake test <target>                 # single target, e.g. xmake test test_store
./run-cppIsekai.sh                   # run (bare boot refuses with usage; --help exit 0)
```

OpenSpec (planning home = repo root):
```bash
openspec list --json                  # active changes
openspec instructions apply --change <name> --json   # tasks + progress
openspec validate --specs             # validate main specs
```

## Architecture

- `src/config/` — registry-backed config core (header-only): 10 knobs {CLI flag (kebab + `--port` legacy alias), `CPPISEKAI_*` env, ini section/key, default, doc comment}. Precedence: default < ini < env < explicit flag. `--dump-config` renders the template; SimpleIni merges save comments in-place. All ini kno knowledge lives here (AGENTS: no config logic outside this dir).
- `src/store/` — sqlite_orm, ONE connection, `data/library.db`; plugin threads never touch sqlite directly (storage.* gate). Skip flags live as `host/skip/<id>` keys in `plugin_state`.
- `src/plugin/` — workbook ABI: JSON-text data contract; single wrap point `call_cached` for stale-serve (`cached:true`); host-wide CURLOPT_PROXY via `plugin::applyProxy`; Lua runtime via sol2 (plugins under `plugins_dev/lua/<site>/`).
- `src/cache/` — disk LRU (per cache_max_mb; evict whole-file on insert overflow) + layout v2 per-plugin tree: `<plugin>/covers/<slug>.<ext>` + `<plugin>/<slug>/chNNN/pNNN.<ext>` (zero-pad order = reading order), `api/<sha>.json` (stale), `img/<sha>` fallback; boot scan = recursive whole-root walk; purge-once gate by `.cache-v2` stamp; slug helper `src/cache/slug.hpp` (fs-safe lowercase, last-8 manga-id suffix on collision).
- `src/http/` — Crow routes; template rendering under `templates/`; image cache route `GET /cache/img`; `POST /config` merge-save; CBZ export route under `cache-layout` (GET `/export/cbz?plugin=&slug=`); stale layer wraps `call_cached`.
- `src/reader/` — window math (pure fn, 10-page sliding window, skip-aware + worker `src/reader/worker.hpp`), in-memory position state, idle worker writes structured pages via `cache::ensure(rel_path=...)`.
- `openspec/` — spec-driven workflow; `specs/` holds main specs per capability; changes go under `changes/<name>/` and archive to `changes/archive/YYYY-MM-DD-<name>/`.

## Conventions

- C++23, `-Wall -Wextra` clean — no new warnings on build; doctest for tests; one test target per module (`tests/test_<module>.cpp`); fixtures under `tests/fixtures/`.
- Header-only modules preferred (config core, slug, reader worker, stale layer); crown facades after source-split-clangdoc: `src/http/{http,http_pages,http_api}.cpp`, `src/plugin/{plugin.cpp,http_client.cpp,lua/,wasm/,js/}`, `src/store/store.cpp`, `src/cache/cache.cpp` -- split units stay <=~200 code-loc (plugin core + http_api documented exceptions, large functions whole); doc-comments are clang-doc (`/** \brief */`) on public decls, why-notes stay `//`.
- All internationalizable content is en_US in code/identifiers; user_chat language = Indonesian (casual).
- Generated files (docs, knowledge, openspec artifacts, reports, briefs): English only — Indonesian appears in user chat, never in committed files.
- Commit subjects: short, lowercase imperative (e.g., `cache: slug pragma header-only (title->folder, collision id suffix) + tests`).
- Branch per change named after change id (e.g., `cli-config`, `cache-layout`, `image-cache`); merge to main with `--no-ff` after review.
- OpenSpec planning artifacts are captured in the change dir per spec-driven schema: `proposal.md`, `design.md` (name decisions D1..Dn), `specs/<capability>/spec.md` (ADDED/MODIFIED/REMOVED delta style), `tasks.md` (checkboxes).
- `.superpowers/sdd/` workspace files: ENGLISH, re-erased after merge (see `R5` prior session).
- Per-skill rule: do NOT copy `<context>`/`<rules>` blocks into artifacts; do NOT reuse artifact-guidance text as file body.
- Worktree hygiene: keep main clean; branch in-flight change; unrelated residue in tree stays uncommitted (restore, don't commit sideways).
- libzippp/CBZ: vendor under `third_party/libzippp`, minizip is a build-level FALLBACK only (see decisions/deps-and-plugins + cache-layout design D6).

## Notes

- MCP tools used in-sesion: `codebase-memory-mcp` (`index_repository` after structural code changes; `search_code` before editing unfamiliar code) and `okf` (`okf_search` / `okf_create` / `okf_update` / `okf_relate` / `okf_validate` strongly preferred over CLI).
- .cache/clangd/ index + build/ artifacts are local only; never commit them (gitignore'd).
