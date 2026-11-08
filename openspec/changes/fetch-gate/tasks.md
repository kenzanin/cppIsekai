# Tasks

## 1. Gate module

- [ ] 1.1 `src/fetch/fetch.{hpp,cpp}`: `Request`/`sync`, sink+retry+applyProxy pindah, satu-satunya `curl_easy_impersonate`; plugin.cpp jadi consumer; full `ctest` green (perilaku identik)
- [ ] 1.2 Opts `profile` threading lua/js/wasm + default live (snapshot conf dibaca saat call; `POST /config` efektif tanpa reload); target invalid => warn + fallback

## 2. Cookie jar

- [ ] 2.1 `cookie_jar` di gate (`CURLOPT_COOKIEFILE/JAR`) + helper `data/plugin-state/<id>.cookies.txt`; off by default; failure => warn non-fatal
- [ ] 2.2 Opts plugin untuk enable cookie; dok singkat di `docs/` atau comment header gate

## 3. Verify

- [ ] 3.1 Unit: fallback target invalid; jar round-trip tmp; isolasi dua jar; full ctest green zero new warnings; `openspec validate --specs`; smoke live opsional `PLUGIN_SMOKE_URL`
