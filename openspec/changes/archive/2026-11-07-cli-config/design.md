# Design: cli-config

## Context

10 knobs total: 7 ini keys today (`server.port/proxy`, `plugin.*` x4, `cache.cache_max_mb`)
plus 3 hardcoded paths in `main.cpp`. CLI11 2.7.2 and SimpleIni 4.27 are already linked;
comment-preserving rewrite is a SimpleIni core feature. `src/config/config.hpp` is an
awaiting stub. User directives: every knob reachable as CLI flag, `CPPISEKAI_*` env
prefix, and ALL config code confined to `src/config/`.

## Decisions

**D1 — one registry, all code in `src/config/` (user directive).** `registry.hpp` holds
the static table: `{section, key, flag, env, type, default, doc-comment}` per knob;
`config.hpp` holds the `Config` struct, `resolve()` (default < ini < env < explicit flag,
env via injectable lookup for tests), `dump()` and `save()`. Header-only, no .cpp — and
no config knowledge outside `src/config/`.

**D2 — CLI11 parses; env is manual.** Flags are generated from the registry (kebab-case;
`--port` stays as legacy alias for `server.port`). Env is NOT wired via CLI11 `envname()`
— it would be indistinguishable from a default. Env applies post-parse, only when
`flag.count()==0`, before ini. Env name = `CPPISEKAI_` + uppered flag sans dashes
(`--plugins-dir` -> `CPPISEKAI_PLUGINS_DIR`).

**D3 — required-config is a manual check, not CLI11 `required()`.** `--dump-config` and
`--help` must work without a config. Post-parse: if `--config` absent and none of
dump/help: print usage + hint, exit 2. Same path for bare `argc==1`. The old silent
default-config boot disappears.

**D4 — dump is generated, comments included.** `[section]` + `; doc comment` +
`key = default`, values escaped minimally. SimpleIni cannot emit comments, so the
template is built from registry strings. Never touches an existing file.

**D5 — save is a merge, not a rewrite.** Load existing ini (or the dump template when
absent), `SetValue` only submitted keys (must exist in registry), `SaveFile`. Body JSON
`{"section.key": value}`; response `{ok, written}`. Comments and ordering survive because
SimpleIni edits in place.

**D6 — `[paths]` section.** `library=data/library.db`, `log_file=data/cppisekai.log`,
`data_dir=data`; `data_dir` is created at boot if missing. Values stay explicit full
paths (no auto-joining) — ponytail: revisit if layout grows.

**D7 — precedence implementation.** Resolve order per knob: registry default, then ini
value if present, then env if set, then flag if count>0. Explicitness is detected via
`CLI11 count()`, not value comparison.

**D8 — settings UI deferred.** This change ships the save endpoint only; a settings page
is a separate future change (no spec exists yet).

Registry map (source of truth for tasks 1.1):

| flag | env | section.key | default |
|---|---|---|---|
| `--port`, `--server-port` | `CPPISEKAI_PORT` | `server.port` | `8090` |
| `--proxy` | `CPPISEKAI_PROXY` | `server.proxy` | *(empty)* |
| `--plugins-dir` | `CPPISEKAI_PLUGINS_DIR` | `plugin.plugins` | `data/plugins` |
| `--fetch-timeout-ms` | `CPPISEKAI_FETCH_TIMEOUT_MS` | `plugin.fetch_timeout_ms` | `30000` |
| `--fetch-profile` | `CPPISEKAI_FETCH_PROFILE` | `plugin.fetch_profile` | `chrome116` |
| `--plugin-timeout-ops` | `CPPISEKAI_PLUGIN_TIMEOUT_OPS` | `plugin.plugin_timeout_ops` | `5000000` |
| `--cache-max-mb` | `CPPISEKAI_CACHE_MAX_MB` | `cache.cache_max_mb` | `2048` |
| `--db` | `CPPISEKAI_DB` | `paths.library` | `data/library.db` |
| `--log-file` | `CPPISEKAI_LOG_FILE` | `paths.log_file` | `data/cppisekai.log` |
| `--data-dir` | `CPPISEKAI_DATA_DIR` | `paths.data_dir` | `data` |

Ponytail ceilings: no short flags in v1 (legacy `--port` only); validation limited to
`port > 0`; save is last-writer-wins per key.
