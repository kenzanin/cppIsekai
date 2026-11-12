# Design

## Context

Audit of `src/plugin/{lua,js,wasm}` against the helpers the five goIsekai port candidates call (`batcave`, `weebcentral`, `comik` — Lua; `komix.to`, `mangapill` — WASM). Full gap list lives in the proposal; this note records the decisions.

## Decisions

- **D1 — One shared C++ URL codec, three thin marshals.** Percent-encoding is fiddly (unreserved set, UTF-8 handling) and every candidate plugin depends on it. A header-only `src/plugin/url_codec.hpp` (encode: unreserved `A-Za-z0-9-_.~` pass through, everything else `%XX` uppercase UTF-8 bytes; decode: `%XX` pairs, `+` is NOT decoded to space — this is `queryEncode` semantics per goIsekai, not form semantics) is exposed as `host.url_encode`/`url_decode` in Lua (replacing the existing Lua-only helper only if semantics diverge — otherwise bind the same core), JS and WASM. Alternative: hand-roll per runtime — rejected, three copies of the same bug surface. Alternative: `curl` escape — rejected, dependency coupling for 20 lines.
- **D2 — `config.get` in JS mirrors Lua exactly.** Reads `config.json` in the plugin's own folder through `plugin::fs_read(base, "config.json")` (already confined), parses with glaze (or the existing lenient helper), missing file/key → nil. No write access; no path beyond the plugin dir. WASM is out of scope for config (no candidate reads it — YAGNI; document in the spec's parity table as "out of scope").
- **D3 — `crypto.vrf_sign` in JS binds the existing shared core.** `src/plugin/vrf.hpp` already exposes a runtime-agnostic `vrf_sign(api_path, params_map, stages)` (C++17, no sol/glaze-in-arguments). JS binding: `host.crypto.vrf_sign(api_path, params, stages)` where `params` is a JS object (string values only — nested objects rejected with the same `"vrf_sign: ..."` error shape), `stages` an array of `{iv, key (std-base64), tbl (std-base64)}`. Error contract mirrors Lua: nil on failure (Lua "returns nil + err", JS "returns null"). Log tagged with plugin id. WASM omitted (no candidate needs it; adding it later is mechanical — same seam).
- **D4 — No `find_list_*` host tool.** `host.html.select/attr` already return all matches; the goIsekai names map 1:1. Recording the mapping in the design doc (this file) and spec instead of adding aliases — YAGNI.
- **D5 — `http.fetch` opts parity is audit-only.** JS and WASM share `make_wasm_fetch` with the Lua front; the audit (a) confirms the opts keys match the spec's `{timeout_ms, profile, method, body, accept, user_agent, headers, cookie}`, (b) confirms JS maps a failed fetch to `null` (Lua `nil`) rather than a JS exception reaching the plugin, (c) confirms reply shape `{status, body, headers}` parses in each runtime. Any divergence found is a bug fix inside this change, not a new tool.
- **D6 — Tests extend the existing fixtures.** `tests/test_plugin.cpp` adds: JS url round-trip + reserved chars; JS config.get present/missing; JS vrf_sign vector vs lua-known answer; WASM url_encode via the wasm fixture import table (add a tiny "url_roundtrip" export to `tests/fixtures/plugins/wasm/fixture.c` or a new `url_probe.wat`-class fixture — decided in implementation). Keep the fixture regeneration note (`clang --target=wasm32 ...`).

## Risks / Trade-offs

- [`url_decode` semantics: `+` vs `%20`] → goIsekai's `url_encode` output is consumed by query parsers expecting `+` = space in some contexts; our `decode` must match what `encode` emits. Both are specified above; verify round-trip in tests rather than guess.
- [vrf_sign JS binding: params value coercion] → Only string values accepted; coercing numbers via String(v) is tempting but creates a silent second contract. Reject non-strings with the error path — same shape as Lua (`tostring()` coercion there exists, keep but document divergence at binding level if unavoidable; prefer parity by rejecting nested/bool/null).
- [WASM binding for url adds imports] → the import table is additive; old wasm plugins still load (missing imports would be a link error — additive imports are safe).

## Open Questions

None — user approved D1, D3, D4 form, and the full scope on 2026-11-12.
