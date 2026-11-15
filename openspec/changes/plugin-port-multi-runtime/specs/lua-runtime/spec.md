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

## ADDED Requirements

### Requirement: Plugin dev layout
Plugins SHALL be developed in `plugins_dev/<lang>/<site>/`; testing SHALL symlink or copy the plugin into `data/plugins/`; the host ini SHALL carry no plugin sections (per `decisions/plugin-dev-layout`). This applies to `lua/` as well as `js/` and `wasm/`.

#### Scenario: Dev plugin reaches the running daemon
- **WHEN** a plugin lives at `plugins_dev/<lang>/<site>/` and is linked into `data/plugins/<id>`
- **THEN** `POST /plugins/reload` picks it up and `/plugin/<id>/<action>` serves it, with no ini entry added
