# Tasks: plugin-host-promotion

## T1 — host core helpers

- [x] 1.1 `normalize_status`: key normalization (lowercase + drop non-alnum) + synonyms `onhold`, `publish`/`published`, `finish`/`finished`, `notyetreleased`; unknown passthrough unchanged (`host_text.hpp`)
- [x] 1.2 split `parse_date(raw) -> optional<time_point>` out of `date_to_iso`; add `date_epoch` (utc `timegm`); assert existing `date_to_iso` vectors are byte-identical (`host_text.hpp`)
- [x] 1.3 `json_blob(text, marker)` — depth + escape-aware balanced scan, nil on absent/truncated (`host_text.hpp`)
- [x] 1.4 `url_abs(base, ref)` — 4 branches per design D6 (`url_codec.hpp` or `src/plugin/url_abs.hpp`)
- [x] 1.5 `re2_capture` shared helper over the existing re2 wrapper (`htmltext_re2.cpp` + decl in `htmltext.hpp`)

## T2 — runtime exposure

- [x] 2.1 Lua: `host.re2.capture`, `host.text.json_blob/date_epoch`, `host.url_abs`; `fetch_body` second return; `get_json(url, opts)` with legacy headers-map fallback (`lua_runtime.cpp`)
- [x] 2.2 JS: `host.re2.capture`, `host.text.json_blob/date_epoch`, `host.url_abs`, `get_json` opts (`js_bindings.cpp`)
- [x] 2.3 WASM untouched — confirm guests link/run with the existing import set (parity table says out of scope)
- [x] 2.4 dedupe `fetch`/`fetch_body` opts parsing if T2.1 makes the duplication in the file worse (the P2 helper from `hot-path-perf` is already in place — reuse it)

## T3 — plugin migration (host already had the tool)

- [x] 3.1 `trim` ×6 → `host.text.trim` (anisascans, comico, comik, dynastyscan, en-thunderscan, fanfox/util)
- [x] 3.2 `normalize_status` ×6 → `host.text.normalize_status` (anisascans, atsumaru, mangadex, mangafire, enrichment-mangadex, mangabay)
- [x] 3.3 `strip_html` ×3 → `host.text.strip_html` (anisascans, atsumaru, mangafire)
- [x] 3.4 `find_all` ×2 → `host.re2.find_all` (anisascans, comik)
- [x] 3.5 `http_body` ×3 → `host.http.fetch_body` (en-thunderscan, mangahere, mangahome) + `get_json` ×5 → `host.http.get_json` (4ucomics, atsumaru, comizy, dynastyscan, enrichment-mangaupdates)

## T4 — plugin migration (new helpers)

- [x] 4.1 `find1` ×13 → `host.re2.capture` (check each call site: raw-block uses of `m[1]` stay on `re2.match`, per #5440)
- [x] 4.2 `http_req` ×12 → `host.http.fetch_body` two-value form
- [x] 4.3 inline JSON-blob scanners ×5 → `host.text.json_blob` (batcave/dle.lua, mangabay/dle.lua, comix, valirscans, mangzio/js)
- [x] 4.4 `epoch`/`date_epoch` ×4 → `host.text.date_epoch` (mangabuddy, mangadex, mangahere/util, mangahome); drop the `MONTHS` tables
- [x] 4.5 `abs_url` ×4 → `host.url_abs` (batcave, comico, madaradex, mangabay); madaradex behavior is canonical

## T5 — shared Lua lib (DLE)

- [x] 5.1 `host.fs.read_lib(rel)` + `require` fallback after the plugin-dir miss, same traversal checks, plugin-local shadows shared (`lua_runtime.cpp`)
- [x] 5.2 `plugins_dev/lib/dle.lua` = union of both forks (mangabay bounds checks + batcave cookie jar/PoW gated on caller options); `dle.json_blob` becomes the D5 passthrough
- [x] 5.3 delete `plugins_dev/lua/{batcave,mangabay}/dle.lua`; each `main.lua` keeps only its site constants; `data/plugins_lib` symlink → `plugins_dev/lib`
- [x] 5.4 note the lib root in `AGENTS.md` plugin bullet (layout stays one line)

## T6 — tests

- [x] 6.1 `tests/test_htmltext.cpp`: `date_epoch` (4 layouts + nil), `json_blob` (absent/nested/escaped/truncated), `url_abs` (4 branches), `normalize_status` (synonyms + unknown passthrough), `re2_capture` (group/no-group/no-match/empty) — 12 cases / 64 asserts
- [x] 6.2 `tests/test_plugin.cpp`: `require` shared fallback, local shadowing, traversal rejection, missing-module message (fixture `lua_libtest` + `tests/fixtures/plugins_lib/*`) — 27 cases / 191 asserts
- [x] 6.3 `tests/test_plugin.cpp` (test_fetch stays socket-free): `fetch_body` pair via hermetic one-shot listener — 404 → `(nil,"upstream status 404")`, refused port → `(nil,msg)`

## Gate

- [x] 7.1 `xmake -y` clean (`-Wall -Wextra`, no new warnings), full suite green
- [x] 7.2 live sweep for every plugin touched in T3/T4/T5: search → detail → chapters → one `/cache/img` page; record the pass table (memory #5362)
- [x] 7.3 DLE pair specifically: batcave behind its PoW guard + mangabay `__DATA__` pages, both through the shared lib
- [x] 7.4 plugin-tree LoC delta recorded (~-490 plugin LoC / ~+130 host LoC) in `proposal.md` after landing
- [x] 7.5 `openspec validate --changes` ✓; no capability removed (spec delta is additive)
