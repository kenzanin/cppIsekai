# lua-runtime delta

## ADDED Requirements

### Requirement: Lua plugin catalog completeness
Every goIsekai Lua site not blocked by Cloudflare SHALL have a ported plugin under `plugins_dev/lua/<site>/` with `main.json` manifest and `main.lua` entrypoint.

#### Scenario: Site not already ported
- **WHEN** a goIsekai `lua/<site>/` plugin exists and is not Cloudflare-blocked and not already in `plugins_dev/lua/`
- **THEN** a `plugins_dev/lua/<site>/` port exists and loads via the Lua (sol2) runtime

#### Scenario: Cloudflare-blocked site
- **WHEN** a goIsekai Lua site is blocked by Cloudflare
- **THEN** it is NOT ported and is listed in the skipped-sites record

## MODIFIED Requirements

### Requirement: Plugin dev layout
Plugins developed in `plugins_dev/<lang>/<site>/`; testing symlinks/copies the plugin into `data/plugins/`; host ini carries no plugin sections (per `decisions/plugin-dev-layout`). Applies to `lua/` as well as `js/` and `wasm/`.
