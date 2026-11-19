# Proposal: plugin-host-promotion

Plugin-tree audit 2026-11-19 (successor to `hot-path-perf` / `store-contention`, both archived).
No site may lose a capability; the ask was "which plugin features belong in the host".

## Why

Census of the live tree: 33 Lua plugins (7,177 LoC), 2 JS (528 LoC), 3 WASM (1,683 LoC).
Cross-plugin function census (name + body diff) found ~42 copies of 11 concepts. Two distinct
causes — and they need different medicine:

**A. Host already has the tool, plugins never migrated.** Proof it is migration debt, not a
missing feature: the two JS plugins use the host surface correctly (`host.text.trim` ×6,
`host.text.strip_html` ×4, `host.text.normalize_status` ×1), while Lua plugins still carry
their own copies.

| helper | copies (files) | LoC | host equivalent |
|---|---|---|---|
| `trim` | 6 — anisascans, comico, comik, dynastyscan, en-thunderscan, fanfox/util | 18 | `host.text.trim` |
| `normalize_status` | 6 — anisascans, atsumaru, mangadex, mangafire, enrichment-mangadex, mangabay | 60 | `host.text.normalize_status` |
| `get_json` | 5 — 4ucomics, atsumaru, comizy, dynastyscan, enrichment-mangaupdates | 35 | `host.http.get_json` |
| `http_body` | 3 — en-thunderscan, mangahere, mangahome | 18 | `host.http.fetch_body` |
| `strip_html` | 3 — anisascans, atsumaru, mangafire | 9 | `host.text.strip_html` |
| `find_all` | 2 — anisascans, comik | 14 | `host.re2.find_all` |

**B. Real host gaps — generic logic that every plugin re-derives.** Each copy is a bug
surface, and the copies have already drifted apart:

| helper | copies | LoC | evidence of drift / risk |
|---|---|---|---|
| `find1` → `re2.capture` | **13** | 78 | 13 byte-identical copies; `re2.match` returns a group table so every plugin writes the same `m[2] or m[1]` shim |
| `http_req` → `fetch_body` err | **12** | 72 | 12 copies of `fetch` + `status~=200` → error; the error string is why each one exists |
| inline JSON-blob scan → `text.json_blob` | 5 — batcave/dle.lua, mangabay/dle.lua, comix, valirscans, mangzio/js | ~120 | **goIsekai parity gap**: both `dle.lua` copies document "host.text.json_blob does not exist here" and inline a balanced-`{}`/`[]` scanner; 13 plugins reference the `__DATA__` marker |
| `epoch` / `date_epoch` | 4 — mangabuddy, mangadex, mangahere/util, mangahome | 50 | four copies of Howard Hinnant civil-days arithmetic + two `MONTHS` tables; `date_to_iso` already parses the same layouts |
| `abs_url` | 4 — batcave, comico, madaradex, mangabay | 24 | **semantics differ**: madaradex handles `//host`, the other three do not; comico tests `sub(1,4)=="http"` |
| `get_json` options gap | — | — | `host.http.get_json(url, headers)` only takes headers, so 4ucomics needs `profile`/`timeout_ms` and writes its own fetcher |

**C. Family-engine duplication (not host code).** `dle.lua` is forked per DLE plugin:
batcave 167 LoC vs mangabay 146 LoC, `diff` = 243 changed lines — the guard/header handling
has already diverged. Same class: mangatown/mangzio share 6 whole functions (madara engine,
2 copies), and the three WASM guests each hand-roll `emit` / `reply_err` / `search_reply` /
`detail_reply` / `chapters_reply` / `pages_reply` — the envelope serializer whose escaping
bugs keep recurring per new plugin.

Site markup, per-site URL shapes and engine knowledge stay plugin-side (memory #5411); what
moves is only what is site-agnostic.

## Scope

- Host core: `src/plugin/host_text.hpp` (date parser split, `date_epoch`, `json_blob`,
  `normalize_status` key table), `src/plugin/url_codec.hpp` + new `url_abs`, `src/plugin/htmltext_re2.cpp`
  (`capture`), `src/plugin/lua/lua_runtime.cpp`, `src/plugin/js/js_bindings.cpp`
  (`get_json` opts, `fetch_body` err, new tools, shared-lib `require` fallback), plus the
  matching WASM-side decision recorded in the spec parity table.
- Plugin tree: 33 Lua plugin files lose their copies; `plugins_dev/lib/dle.lua` becomes the
  single DLE lib (`data/plugins_lib` symlink), per-plugin forks deleted.
- Spec delta on `plugin-host-tools` — no other capability changes.

## Non-goals

- No site markup, selectors, URL shapes or per-site guard logic in the host (#5411).
- No `unpack_packer` promotion (fanfox + mangahere only; Dean-Edwards unpacker, YAGNI until a 3rd site).
- No JS module loader for the mangatown/mangzio madara pair (defer; Lua `require` is the proven
  seam and no 3rd JS plugin exists).
- No WASM guest shared header in this change (needs a shared include dir in the guest
  `build.sh`; track separately as `wasm-guest-abi-header`).
- No change to `re2.find`/`find_all` 1-based semantics (#5440) — `capture` is additive.

## Verification

- `xmake -y` clean (`-Wall -Wextra`), full suite green, `openspec validate --changes` ✓.
- New unit cases for every promoted helper (tables, parse failures, empty input, marker absent,
  `//host` refs, drifted synonyms).
- Per-plugin live sweep for every plugin touched: search → detail → chapters → one
  `/cache/img` page (memory #5362 — green build is not evidence). DLE pair (batcave, mangabay)
  verified through the shared lib specifically, including their PoW guard and `__DATA__` pages.
- Plugin tree LoC delta recorded before/after (~-490 plugin LoC, ~+130 host LoC).
