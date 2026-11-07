# Proposal: cli-config

## Why

Ops and debugging happen through curl and flags: every host knob must be reachable as a
CLI flag without editing files, and the ini must stay the durable, human-edited source.
Today only `--port`/`--config` exist, three paths are hardcoded in `main.cpp`, there is no
way to inspect the expected configuration, and running bare boots silently with defaults.

## What Changes

- Single config registry (`src/config/`): every knob declared once with ini section/key,
  kebab-case CLI flag, `CPPISEKAI_*` env name, default, and doc comment.
- `--config <file>` becomes required (except `--help` / `--dump-config`); no args or args
  without `--config` print usage with a `--help` hint and exit non-zero, no boot.
- Precedence: built-in default < ini < `CPPISEKAI_*` env < explicit flag. The legacy bare
  `PORT` env is replaced by `CPPISEKAI_PORT`.
- `--dump-config [path]`: commented ini template generated from registry defaults; works
  without `--config`; stdout by default, file when a path is given.
- `POST /config` merges values into the ini via SimpleIni, preserving comments and
  formatting (backend only — a settings UI page is a separate future change).
- New `[paths]` section (`library_db`, `log_file`, `data_dir`) replacing the hardcoded
  `data/...` values.

## Capabilities

### New Capabilities
- `cli-config` — required config file, layered precedence (ini/env/flag), self-describing
  dump, comment-preserving save endpoint.

## Impact

- New `src/config/` (registry, resolve, dump/save); `src/main.cpp` rewired — scattered
  `Get*Value` calls replaced by the resolved `Config`; `src/http/http.cpp` +1 route.
- Env surface renamed to `CPPISEKAI_*`; `data/cppisekai.ini` gains documented `[paths]`.
- Tests: precedence unit (injectable env lookup), dump snapshot, save round-trip that
  keeps comments intact.
