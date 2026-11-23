# Tasks: ui-action-feedback

## T1 — shared primitive

- [x] 1.1 Create `templates/feedback.mustache`: `#toast-host`, `window.toast(kind, message)` (success/error/info, 4-toast cap, auto-dismiss, click to dismiss), and `window.runFeedback(job, btn)` (busy LED, disabled button, stale-error clear, `{ok,error}` → toast, LED result for 3 s, exception → error toast).
- [x] 1.2 Include the partial in `templates/top.mustache` and `templates/reader_head.mustache`.

## T2 — detail page wiring (static/detail.js + templates/detail.mustache)

- [x] 2.1 `refresh()` — success/error toast, clear stale `err`, LED on the Update button.
- [x] 2.2 `toggleLibrary()` / `togglePin()` — add/remove/pin toasts, clear stale `err`.
- [x] 2.3 chapter flags: `toggleRead`, `markAllRead`, `markPrevRead`, `toggleDone` — one toast per batch (not per row), LED on the clicked control.
- [x] 2.4 enrichment: fetch/force per source, reset detail/cover, `saveChosen` (pick/toggle/add-all/remove-all) — success/error toast with source id.
- [x] 2.5 dim+blur toggle in `templates/top.mustache` — success/error toast.

## T3 — other pages

- [x] 3.1 Settings: `save()`, `restart()`, `discard()` in `templates/settings_script.mustache` — toasts + LED on Save/Restart.
- [x] 3.2 Plugins: `toggle`, `clearCache`, `reload` in `templates/plugins_script.mustache` — toasts + LED.
- [x] 3.3 Migrate: `run()` and `migrate()` in `templates/migrate.mustache` — search-empty is info, migrate success/error is success/error.
- [x] 3.4 Reader footer restart in `templates/reader_foot.mustache` — info toast before the request drops the connection.

## T4 — goldens + gates

- [x] 4.1 Two-step golden re-lock: `CPPISEKAI_UPDATE_GOLDEN=1 xmake test test_templates` (expect red) then `CPPISEKAI_UPDATE_GOLDEN_ACK=1 xmake test test_templates` (green); review `git diff tests/fixtures/golden` — only the feedback partial / LED attributes.
- [x] 4.2 `xmake -y` clean (clang, zero new warnings) and `xmake test` full suite green; `test_gridjs` still passes.
- [x] 4.3 Browser verification in the real Chrome: one success toast (e.g. pin) and one failure toast (e.g. Update on a disabled plugin), LED returns to idle, button re-enabled, stale `err` line cleared.

## T5 — knowledge

- [x] 5.1 Record the reusable feedback contract in OKF via okf MCP (`decisions/ui-action-feedback`, valid `generated.by = agent/mcp`, related to `architecture/reader-fullscreen` or the nearest UI concept), and report `okf_validate --strict` result.
