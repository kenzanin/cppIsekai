# Design: plugin-host-promotion

Invariant: the host stays site-agnostic (#5411/#5732). Everything below is site-neutral
string/HTTP/URL/loader mechanics. Site markup, selectors, URL shapes and guard logic stay in
plugins.

## D0 — `normalize_status` key normalization (migration prerequisite)

Host map is keyed on the exact lowercase word today, while the 6 plugin copies key on
`alnum-stripped lowercase` (so `"ON-GOING"` → `"ongoing"`). Without this, migrating them would
change returned values.

`host.text.normalize_status` normalizes the **key** (lowercase + drop non-alnum) before lookup,
and the synonym table grows by the values those copies already handle: `ongoing`, `onhold`
→ On Hiatus, `publish`/`published` → Publishing, `finish`/`finished` → Completed,
`notyetreleased` → Not Yet Released. Unknown values pass through **unchanged** (no lowercasing
of the passthrough — #5411 host-owns-vocabulary, plugin-passed raw string survives).

Regression guard: existing `tests/test_htmltext.cpp` vectors must stay green; new cases cover
`ON-GOING`, `On Hiatus`, `finished`, unknown `"Dropped by author"`.

## D1 — `re2.capture(pattern, text)` → string | nil

The 13 `find1` copies are all `local m = host.re2.match(p, s); return m and (m[2] or m[1])`.
Promote exactly that: return capture group 1 when the pattern has groups, else the full match,
`nil` when no match. Implemented once in `htmltext_re2.cpp` (`re2_capture`) over the existing
re2 wrapper; Lua and JS marshal the same function. Additive — `re2.match`/`find`/`find_all`
1-based semantics unchanged (#5440).

Edge: a pattern matching an empty group returns `""` (a hit), never `nil`.

## D2 — `http.fetch_body(url, opts)` → `(body, err)`

Today it returns `body|nil`, so the 12 `http_req` copies exist only to turn "nil" into
"upstream status N". It returns a second value: `nil` on success, otherwise
`"upstream status <code>"` (or the transport error). Host logs it tagged with the plugin id.
Existing single-value callers are unaffected (Lua ignores extra returns).

## D3 — `http.get_json(url, opts)`

Lua's version takes a flat headers map; it moves to the **same opts table as `fetch_body`**
(`headers`, `timeout_ms`, `profile`, `cookie`, `accept`, `method`, `body`). Compatibility rule
(no caller breakage, zero current callers verified): if the table contains none of the
recognized opts keys, treat it as a headers map. Returns `decoded_table, err`. This is the
4ucomics blocker (needs `profile`/`timeout_ms` on an API fetch).

## D4 — `text.date_epoch(raw)` → number | nil

`date_to_iso` already parses these layouts; split that into `parse_date(raw)` (returns
`optional<time_point>`) and keep `date_to_iso` as the formatter over it — one parser, two
tools. `date_epoch` returns `timegm` seconds (the copies assume UTC; no local-time drift).
Accepted layouts: `YYYY-MM-DD`, `YYYY-MM-DDTHH:MM:SS` with optional `Z`/`±HH:MM`/`.ms`,
`Mon DD, YYYY`, `DD Mon YYYY`, `HH:MM`-suffixed variants. Unparsable → `nil`, never `now()`.

Removes 4 copies incl. two `MONTHS` tables and the Hinnant civil-days code; kills the
"chapter dates differ per plugin" class.

## D5 — `text.json_blob(text, marker)` → string | nil

Returns the first balanced `{...}` or `[...]` at or after `marker`, scanning with string-escape
awareness (`"` toggles, `\` escapes the next byte), `nil` when the marker or a balanced body is
absent. This is goIsekai's `host.text.json_blob`; both `dle.lua` forks document its absence and
inline it, comix/valirscans/mangzio carry their own. Returns raw JSON text — decoding stays
`host.json`.

Nesting depth is tracked, not escaped-string-naive; truncated input (CDN cut) returns `nil`
rather than a partial blob.

## D6 — `url_abs(base, ref)` → string

Four copies with three different behaviors; adopt the most correct (madaradex) as canonical:

1. `ref` starts with `http://` / `https://` → returned unchanged.
2. `ref` starts with `//` → base scheme + ref.
3. `ref` starts with `/` → base origin + ref.
4. otherwise → base truncated at the last `/` + ref.

No dot-segment normalization and no query/fragment merging — documented ceiling (all four call
sites pass site-absolute paths; callers needing `../` stay plugin-side).

## D7 — shared Lua lib: `require` fallback to a read-only lib root

`require` is already a confined pure-Lua loader over `host.fs.read(dir, rel)`
(`lua_runtime.cpp:118-148`). It gains one fallback, after the plugin-dir lookup fails:
`host.fs.read_lib(rel)` reads `<plugins_dir>/../plugins_lib/<rel>.lua` (or `/init.lua`), with
the same traversal checks and no write access. Plugin-local wins over shared for the same name;
the per-state cache keeps the resolved module per name.

Sandbox unchanged: two read-only roots, no path outside them, no host code growth per plugin.
Dev wiring: `plugins_dev/lib/` with `data/plugins_lib` symlinked to it, mirroring the existing
`data/plugins → plugins_dev/lua` link.

## D8 — one `dle.lua` for DLE plugins

`plugins_dev/lib/dle.lua` = the union of the two forks: mangabay's bounds/index checks plus
batcave's cookie-jar + PoW guard, with those behaviors gated on the options the caller passes
(`cookie = true`, `pow = true`). Each plugin keeps a thin wrapper holding its own
host/base/header constants — the site-specific part. Both forks' comments about the missing
`host.text.json_blob` are obsolete once D5 lands; `dle.json_blob` becomes a direct passthrough.

Acceptance is behavioral, not textual: batcave (behind its PoW guard) and mangabay
(`window.__DATA__` pages) must both do search → detail → chapters → page image after the move.

## Deferred (recorded so it is not re-litigated)

- **WASM guest reply header** — the right fix (3 copies of `emit`/`reply_err`/`*_reply`), but it
  needs a shared include dir wired into every guest `build.sh`; separate change
  `wasm-guest-abi-header`. No new WASM plugin is coming before then.
- **JS madara engine** — mangatown/mangzio share 6 functions; 2 sites is YAGNI until a 3rd JS
  plugin, and JS has no confined loader seam yet.
- **`unpack_packer`** — fanfox + mangahere only.

## Test hook

- `tests/test_htmltext.cpp`: `date_epoch` (4 layouts + nil), `json_blob` (marker absent,
  nested objects, escaped braces inside strings, truncated), `url_abs` (4 branches),
  `normalize_status` (new synonyms + unknown passthrough), `re2.capture` (group / no group /
  no match / empty group).
- `tests/test_plugin.cpp`: `require` fallback — plugin-dir module still shadows the shared one;
  traversal (`require("..x")`, `require("/x")`) rejected; missing module error names it.
- `tests/test_fetch.cpp`: `fetch_body` second return on a 4xx and on transport failure.
