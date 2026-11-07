---
type: decision
title: "cli-config: registry-backed config surface (CLI/env/ini/dump/save)"
description: "Decision: config registry single-source di src/config/ (10 knob: flag kebab + CPPISEKAI_* env + ini key + doc) dengan precedence default<ini<env<flag, --config required (help/dump exempt, exit 2 tanpa boot), --dump-config template ber-komentar dari registry, POST /config comment-safe merge (SimpleIni), [paths] menggantikan hardcoded data paths."
tags: [config, cli, registry, ini, opr, api-surface]
generated: { by: agent/mcp, at: "2026-11-07T14:04:29Z" }
status: stable
---

## The problem

CLI config dan operabilitas: hanya ada `--port`/`--config`, tiga path hardcoded di main.cpp, tidak ada cara melihat konfigurasi yang diharapkan, dan login minimal boot default diam-diam.

## Design yang diambil

**1. Satu registry, seluruh kode di `src/config/` (D1, arahan user).** `registry.hpp` = tabel 10 knob (`{section, key, flag, alias, env, def, doc, type}`); `config.hpp` = struct `Config`, `build_cli(CLI11)` bind per entry, `resolve()` default<ini<env<flag (env injectable utk test), `dump()` template ber-komentar, `save()` merge SimpleIni comment-safe, `parse_updates()` koersi body JSON -> string (crow-free, testable). Header-only.

**2. Precedence (D2/D7):** default < ini < `CPPISEKAI_*` env < flag eksplisit (deteksi via `count()>0`, bukan value compare). Bare `PORT` env dipensiunkan; env = `CPPISEKAI_` + flag uppered tanpa dashes.

**3. `--config` required (D3) manual post-parse check — bukan CLI11 required():** `--help` dan `--dump-config` dikecualikan; tanpa config = usage + hint `--help`, exit 2, tanpa boot/DB/`data/`.

**4. `--dump-config [path]` (D4):** template dari registry (`[section]` + `; doc` + `key = default`), stdout default / file via arg, tanpa `--config`, tidak pernah menimpa config existing.

**5. `POST /config` (D5):** merge via SimpleIni in-place (komentar & urutan selamat), file hilang = seeded dari dump template, unknown key ditolak validation-first, response `{ok, written}` / `{ok:false, error}` (corrupt file -> 500 class).

**6. `[paths]` (D6):** `library=data/library.db`, `log_file=data/cppisekai.log`, `data_dir=data` (dibuat saat boot bila hilang) — menggantikan 3 hardcoded path.

**7. UI settings di-defer** — endpoint save backend-only di change ini; halaman settings = change terpisah (belum ada spec).

## Registry (10 knob)

| flag | env | ini |
|---|---|---|
| --port/--server-port | CPPISEKAI_PORT | server.port=8090 |
| --proxy | CPPISEKAI_PROXY | server.proxy=(empty) |
| --plugins-dir | CPPISEKAI_PLUGINS_DIR | plugin.plugins=data/plugins |
| --fetch-timeout-ms | CPPISEKAI_FETCH_TIMEOUT_MS | plugin.fetch_timeout_ms=30000 |
| --fetch-profile | CPPISEKAI_FETCH_PROFILE | plugin.fetch_profile=chrome116 |
| --plugin-timeout-ops | CPPISEKAI_PLUGIN_TIMEOUT_OPS | plugin.plugin_timeout_ops=5000000 |
| --cache-max-mb | CPPISEKAI_CACHE_MAX_MB | cache.cache_max_mb=2048 |
| --db | CPPISEKAI_DB | paths.library=data/library.db |
| --log-file | CPPISEKAI_LOG_FILE | paths.log_file=data/cppisekai.log |
| --data-dir | CPPISEKAI_DATA_DIR | paths.data_dir=data |

## Verification (2026-11-07)

- ctest 8/8 (test_config 4 cases / 132 assertions: precedence, dump, save round-trip, parse_updates koersi termasuk 2^53 exact-below / ponytail-f64-above).
- Reviewer independen whole-diff: 0 Critical; I1 (untested coercion) FIX landed `28245fb`; 3 minor deferred (corrupt-ini save 400 vs 5xx, SaveFile non-atomic, --help tanpa default).
- Live smoke binary asli: bare/flags-tanpa-config exit 2 tanpa boot; precedence live 8092>8093>8091; dump->edit->run bootstrap :8095; POST /config save round-trip port 8096 + komentar utuh, unknown key ditolak tanpa write.

## Ponytail ceilings

f64 > 2^53 kehilangan exact-int di koersi (upgrade bila glaze punya path int64); lenient numeric read silent-default; `--help` belum menampilkan default per knob; SaveFile non-atomic.

# Related Concepts
- [cppIsekai core architecture: plugin-per-site manga viewer](manga-viewer-core.md): host architecture the config layer feeds (paths/port feed store/log/plugin/cache wiring).
- [Dependencies & plugins: curl-first APIs, runtime parity, no hand-rolled crypto](../decisions/deps-and-plugins.md): curl-first API ruling underpins POST /config being a first-class REST route.
- [cppIsekai dependency API surface + MangaDex auth resolution](../references/dependency-api.md): per-dependency API ground the registry wraps (CLI11, SimpleIni comment-preserving rewrite, glaze generic JSON).
