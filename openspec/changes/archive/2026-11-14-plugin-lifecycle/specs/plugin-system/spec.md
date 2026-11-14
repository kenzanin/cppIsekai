# Spec Delta: plugin-system

## ADDED Requirements

### Requirement: Disabled plugins are inert
A plugin whose `enabled` flag is 0 SHALL be omitted from the search source picker, skipped by
the auto-update scheduler's candidate query, and SHALL refuse on-demand refresh. The plugin
SHALL stay installed and listed, so the flag is a pause rather than an uninstall; library,
reader and saved-entry paths SHALL keep working for titles already saved under it.

#### Scenario: Search picker omits a disabled plugin
- **WHEN** a plugin is disabled and the search page renders its source picker
- **THEN** the disabled plugin is not offered as a search source

#### Scenario: Refresh refused
- **WHEN** `POST /api/refresh/<plugin>/<slug>` targets a disabled plugin
- **THEN** the response is `{"ok":false,"error":"plugin disabled"}` and no plugin call is made

#### Scenario: Scheduler skips a disabled plugin
- **WHEN** the auto-update scheduler builds its candidate list
- **THEN** saved entries belonging to a disabled plugin are not selected for refresh

#### Scenario: Re-enabling resumes work
- **WHEN** the `enabled` flag is set back to 1
- **THEN** the plugin is eligible for scheduled and on-demand refresh again, with no reload required

### Requirement: Removed plugin folders are purged
When a scan finds that a previously registered plugin id has no folder on disk, the host
SHALL forget it in a single transaction: `title`, `plugin_state`, `reader_positions`,
`saved_manga`, `title_meta`, `update_attempts` and `api_cache` rows for that plugin, then the
plugin row last. A missing plugins directory SHALL short-circuit before the prune, so an
unavailable mount cannot wipe the registry.

#### Scenario: Folder removed between boots
- **WHEN** a plugin's folder is gone on the next scan
- **THEN** its rows are purged in one transaction and the plugin no longer appears in the list

#### Scenario: Missing directory does not prune
- **WHEN** the configured plugins directory does not exist at boot
- **THEN** no plugin is forgotten
