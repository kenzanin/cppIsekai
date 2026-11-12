# Tasks

## 1. Disabled semantics

- [x] 1.1 Auto-update scheduler skips disabled plugins (`update_candidates` gains an `EXISTS (… enabled)` filter). Verify: store/unit level.
- [x] 1.2 `POST /api/refresh/<plugin>/<slug>` answers `{"ok":false,"error":"plugin disabled"}` for disabled plugins. Verify: test.
- [x] 1.3 Verify search picker hides disabled (existing client filter) and library/reader paths stay ungated. Verify: inspection note in the apply log.

## 2. Folder-removal GC

- [x] 2.1 `Store::plugin_forget(plugin_id)`: one-transaction cascade — title (via registry id), plugin_state, reader_positions, saved_manga, title_meta, update_attempts, api_cache, plugin row last. Verify: store test.
- [x] 2.2 Scan prune: registry ids absent from the fresh scan are forgotten with a log line; missing dir short-circuits before the prune. Verify: boot/scan path.

## 3. Wrap

- [ ] 3.1 Full suite green, daemon restarted, disabled + removed flows exercised by hand.
