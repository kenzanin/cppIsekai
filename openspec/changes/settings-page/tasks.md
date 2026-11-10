# Tasks

## 1. Read projection
- [x] 1.1 `src/config/`: add `describe()` (or equivalent) returning `{section, key, fullKey, doc, type, flag, env, default, value, source}` per registry entry, re-resolved per call; keep it header-only with the rest of the core. `source` = winning layer (default|ini|env|flag). No config logic outside `src/config/`.
- [x] 1.2 `GET /config` in `src/http/http_api.cpp` returning the projection as JSON (`{ok:true, knobs:[...]}`). POST route unchanged.
- [x] 1.3 Unit test: describe() layering — ini beats default, env beats ini, flag beats env; bad numeric ini value falls back to default; resolver does not write the file.

## 2. Settings page
- [x] 2.1 `templates/settings.mustache`: sections from the registry, one row each — doc, input, source badge, read-only when env/flag-won, restart badge per D4. Alpine; submits dirty fields only; re-GETs after save; server error on the page `err` line.
- [x] 2.2 Enum `<select>`s for `image_format`, `log.level`; `server.proxy` as password input with reveal toggle; numerics as `type=number`. Client does no type validation beyond that.
- [x] 2.3 `register_pages`: `/settings` route (renders the shell; page fetches `GET /config` on boot); `templates/top.mustache`: `Settings` nav entry after Plugins (`active_settings`).

## 3. Verification
- [x] 3.1 Zero-warning build; `ctest` green (config tests extended).
- [x] 3.2 `openspec validate` clean for this change.
- [x] 3.3 Live smoke: open `/settings`, edit `cache.cover_max_px`, save, confirm ini file updated with comments intact and `GET /config` reflects it; set `CPPISEKAI_PORT` in the launch env, confirm that row renders read-only; bad value (`cache_max_mb=abc`) shows server error and file untouched; nav link works from every page.
