# Tasks

## 1. Disabled semantics

- [x] 1.1 Auto-update scheduler skips disabled plugins (`update_candidates` gains an `EXISTS (… enabled)` filter). Verify: store/unit level.
- [x] 1.2 `POST /api/refresh/<plugin>/<slug>` answers `{"ok":false,"error":"plugin disabled"}` for disabled plugins. Verify: test.
- [x] 1.3 Verify search picker hides disabled (existing client filter) and library/reader paths stay ungated. Verify: inspection note in the apply log.

## 2. Folder-removal GC

- [x] 2.1 `Store::plugin_forget(plugin_id)`: one-transaction cascade — title (via registry id), plugin_state, reader_positions, saved_manga, title_meta, update_attempts, api_cache, plugin row last. Verify: store test.
- [x] 2.2 Scan prune: registry ids absent from the fresh scan are forgotten with a log line; missing dir short-circuits before the prune. Verify: boot/scan path.

## 3. Wrap

- [x] 3.1 Full suite green, daemon restarted, disabled + removed flows exercised by hand.
  - verified 2026-11-14: full suite green (18/18 targets on the same build). Freshly restarted daemon on :8090 — disabled flow: `POST /plugin/6/enabled {"enabled":false}` → `{"enabled":false,"id":6}` (db flag 0), `POST /api/refresh/atsumaru/some-slug` → `{"ok":false,"error":"plugin disabled"}`, then re-enabled (db flag 1). Removed flow: deleted the `data/plugins/atsumaru` symlink and restarted — log line `[plugin] atsumaru: folder gone, registry + traces purged`, plugin row id 6 gone, `atsumaru` absent from `/api/plugins`; restoring the symlink and restarting re-registered it as id 14.
