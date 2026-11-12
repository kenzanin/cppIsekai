# Spec Delta

## ADDED Requirements

### Requirement: Disabled plugin semantics

A disabled registry plugin SHALL disappear from the search source picker, be
skipped by the auto-update scheduler, and reject manual refresh calls, while
remaining listed in the library and readable from cache.

#### Scenario: Disabled plugin is quiet but readable

- **WHEN** a plugin is disabled on the plugins page
- **THEN** the search picker omits it, the scheduler never queues its titles,
  its refresh endpoint answers ok:false `plugin disabled`, and its saved
  manga remain readable from cached pages

### Requirement: Removed plugin folder garbage-collects the registry

When a plugin folder disappears, the next scan SHALL drop the plugin's
registry row and every child trace (title, plugin_state, reader_positions,
saved_manga, title_meta, update_attempts, api_cache) in one transaction.

#### Scenario: Folder deleted

- **WHEN** a plugin folder is removed and the plugin scan re-runs
- **THEN** no row referencing that plugin id remains in any of the tables
  above, and the removal is logged
