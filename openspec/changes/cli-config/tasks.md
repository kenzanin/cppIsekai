# Tasks: cli-config

## 1. Config core (all code under `src/config/` — user directive)
- [x] 1.1 `src/config/registry.hpp` + `config.hpp`: the 10-knob registry (table in design), `Config` struct, `resolve()` default<ini<env<flag with injectable env lookup; unit test precedence (tmp ini, fake env, explicit flag) + unset-flag-keeps-ini
- [x] 1.2 `main.cpp` rewire: drop scattered `Get*Value` calls, `--config` required + no-args usage exit 2, `CPPISEKAI_*` env (legacy bare `PORT` dropped), `--port` alias kept, `[paths]` knobs replace hardcoded data/library.db, cppisekai.log, data/

## 2. Dump & save
- [x] 2.1 `--dump-config [path]` generated from the registry (doc comment per key, default values; stdout default, file via arg; runs without `--config`; never touches an existing config)
- [x] 2.2 `POST /config` comment-safe save (merge submitted keys via SimpleIni, unknown keys rejected; missing file created from dump template) + unit test round-trip keeping comments

## 3. Verification
- [x] 3.1 ctest green; `openspec validate`
- [x] 3.2 smoke: no-args usage; flag>env>ini live check (`CPPISEKAI_PORT` vs `--port` vs ini); dump->edit->run bootstrap; save round-trip preserves comments
