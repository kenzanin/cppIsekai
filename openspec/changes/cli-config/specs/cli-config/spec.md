# cli-config delta

## ADDED Requirements

### Requirement: Required config argument
The host SHALL require `--config <path>` on every boot except `--help` and
`--dump-config`. Invoked with no arguments — or with arguments lacking `--config` — it
SHALL print a usage line (where to point the ini, plus a `--help` hint) and exit
non-zero, without opening sockets or the database.

#### Scenario: No arguments
- **WHEN** the binary runs with no arguments
- **THEN** usage plus a `--help` hint is printed and the exit code is non-zero

#### Scenario: Arguments without config
- **WHEN** flags are passed, `--config` is absent, and neither `--dump-config` nor
  `--help` is used
- **THEN** the same usage path runs and the host does not boot

### Requirement: Layered configuration precedence
Every registry knob SHALL resolve: built-in default, overridden by the ini value,
overridden by the `CPPISEKAI_*` env variable, overridden by an explicitly passed flag. A
flag that is not passed SHALL NOT override the ini. Env names are upper-snake with the
`CPPISEKAI_` prefix (`CPPISEKAI_PORT`, `CPPISEKAI_PROXY`); the legacy bare `PORT` is no
longer read.

#### Scenario: Flag beats env beats ini
- **WHEN** `server.port` is set in the ini, `CPPISEKAI_PORT` in the environment, and
  `--port 9000` on the command line
- **THEN** the listener binds 9000; without `--port` it binds the env value; without the
  env it binds the ini value

#### Scenario: Unset flag keeps ini
- **WHEN** `cache_max_mb` is set in the ini and no `--cache-max-mb` is passed
- **THEN** the cache cap comes from the ini, not the built-in default

### Requirement: Self-describing dump
`--dump-config [path]` SHALL emit every registry entry as its ini section, a doc comment,
and `key = default` — generated from the registry, never hand-maintained. It SHALL run
without `--config`, print to stdout by default, write the file when a path is given, and
never modify an existing config file.

#### Scenario: Dump lists every knob
- **WHEN** `--dump-config` runs with no path
- **THEN** stdout contains every registry key under its section, each preceded by its doc
  comment, with default values

#### Scenario: Dump to file
- **WHEN** `--dump-config out.ini` runs
- **THEN** the same content is written to `out.ini` instead of stdout

### Requirement: Comment-preserving save endpoint
`POST /config` SHALL merge submitted values into the ini through SimpleIni so existing
comments and ordering survive; when the configured file does not exist it SHALL be
created from the dump template. The endpoint is backend-only in this change; any
settings UI is a separate future change.

#### Scenario: Save keeps comments
- **WHEN** the ini carries comments and `POST /config` sets `server.port`
- **THEN** the file keeps its comments and layout with only that value changed

#### Scenario: Save creates missing file
- **WHEN** the configured ini does not exist and a save arrives
- **THEN** the file is created from the dump template containing the submitted values
