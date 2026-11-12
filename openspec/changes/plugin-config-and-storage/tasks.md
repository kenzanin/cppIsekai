# Tasks: plugin-config-and-storage

## 1. Host: storage backend
- [ ] 1.1 Add `plugin_state(plugin_id, key, value)` table + Store get/set methods (schema migration)

## 2. Host: host-tool bindings
- [ ] 2.1 Extend `http.fetch` with `method`, `body`, `headers` options (custom request, headers merged after built-ins)
- [ ] 2.2 Add `config.get(key)` — parse plugin-folder `config.json` once per load, cache, nil on missing
- [ ] 2.3 Add `storage.get`/`storage.set` bindings wired to Store plugin_state

## 3. Mangadex plugin
- [ ] 3.1 Add `config.example.json` + `.gitignore` entry for `data/plugins/*/config.json`
- [ ] 3.2 Auth logic in `main.lua`: login (password flow), token caching in storage with `expires_at`, refresh flow, 401 retry-once, Authorization header only on api.mangadex.org

## 4. Verification
- [ ] 4.1 Unauth path: rebuild, run host, search/detail/chapters behave as before (no auth attempts in logs)
- [ ] 4.2 Auth path: with real personal-client `config.json`, login succeeds, token refreshes after 15 min (or forced `expires_at`), 401 retry works
- [ ] 4.3 Storage: set → host restart → get returns value
