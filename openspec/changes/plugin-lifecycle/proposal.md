# Plugin lifecycle: disabled semantics + folder-removal GC

## Why

The registry `enabled` flag exists (plugins-page toggle, one call-path gate on
the debug route) but half the surface ignores it, and a plugin folder deleted
on disk leaves a stale registry row plus orphaned rows in every child table.

## What

Disabled plugins (registry `enabled = 0`):

1. disappear from the search source picker (client already filters on
   `/api/plugins` `enabled` — verify only);
2. are skipped by the auto-update scheduler;
3. have manual update/refresh answered `ok:false "plugin disabled"`;
4. stay listed in the library and readable from cache like offline (no
   library/reader behavior change);
5. a deleted plugin folder drops the registry row AND every child trace
   (title, plugin_state, reader_positions, saved_manga, title_meta,
   update_attempts, api_cache) on the next scan.

## Non-goals

- Disk cache GC on forget: cache dirs are regenerable and keyed by plugin
  folder; orphans age out. Operator can `rm -rf` the plugin's cache tree.
- A dedicated delete endpoint: folder removal IS the delete gesture.
