# Design

- **D1 — the gate is one predicate, not a distribution.**
  `Store::plugin_enabled(plugin_id)` stays the single disabled-test. Call
  sites: (a) the debug/ABI route (existing), (b) the update scheduler batch
  (new guard), (c) `POST /api/refresh` (new). The search picker filters
  client-side on the `/api/plugins` `enabled` field (already shipped); the
  library grid and reader never gate — disabled must stay readable.

- **D2 — folder-removal GC rides the existing scan.** After `scan_dir` fills
  `fresh`, any registry `plugin_id` absent from `fresh` is forgotten and
  logged. A missing plugin dir short-circuits before the prune, so a mount
  glitch cannot wipe the registry; an emptied dir legitimately does.

- **D3 — cascade = every table with a plugin column, one transaction.**
  `title` (via the registry row id), `plugin_state`, `reader_positions`,
  `saved_manga`, `title_meta`, `update_attempts`, `api_cache`, then the
  `plugin` row itself. sqlite_orm `remove_all` per table inside a transaction;
  children first.

- **D4 — no new endpoints.** Folder removal is the delete gesture; the
  registry prune is scan-driven, exactly like the upsert it mirrors.
