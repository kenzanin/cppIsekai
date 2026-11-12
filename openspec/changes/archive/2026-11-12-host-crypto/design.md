# Design

## Context

See proposal.md — Why. Mangafire signs every API request with a site-specific VRF transform; without it the upstream answers 403. cppIsekai has no crypto host tool (OKF `decisions/host-url-codec` deferred one as YAGNI until a plugin needed it). The reference implementation is goIsekai's `internal/pluginutil/vrf_sign.go` (algorithm already studied; OKF `references/mangafire-vrf-algorithm`). Binding goes into the Lua runtime first, sharing a pure C++ transform so JS/wasm bindings can reuse it later without duplication.

## Goals / Non-Goals

**Goals:**
- `host.crypto.vrf_sign(api_path, params, stages)` available to Lua plugins, byte-for-byte compatible with the goIsekai algorithm.
- Plugin declares VRF tables (`iv`, `key`, `tbl`) as data; host executes only the transform.
- Deterministic unit tests against ported vectors.

**Non-Goals:**
- No general crypto surface (no md5/sha/aes) — only `vrf_sign`; more lands when a plugin needs it.
- No JS/wasm binding in this change (the pure C++ seam keeps that a follow-up).
- No change to mangafire plugin itself (separate port task).
- No caching/memoizing of signatures.

## Decisions

- **Pure header-only transform `src/plugin/vrf.hpp`, `vrf_sign(path, params_map, stages)` — no class, no globals.** Fits the project's header-only preference and the runtime-agnostic seam (lua_call_entry pattern); JS/wasm can bind the same function later. Alternative: inline into `lua_runtime.cpp` — rejected, would duplicate when the second binding appears.

- **base64 via vendored glaze (`third_party/glaze/glaze/base64`)** for std-base64 decode (`key`/`tbl`); RawURL base64url output gets a tiny local encoder (URL-safe alphabet, no padding) since glaze emits RFC 4648 with `=`. Alternative: stbrumme/hash-library — rejected, nothing hashes here (VRF is XOR/table, not a digest). Alternative: hand-rolled decoder/encoder for both — rejected, half of it already exists and is tested.

- **Lua entry returns `(result, err)` pair like `host.http.fetch`** — `crypto.vrf_sign(...)` yields `(string, nil)` on success or `(nil, "vrf_sign: stage <n>: ...")` on bad input. Follows the established host-tool error contract; plugins already pcall-pattern their JSON decode. Alternative: throw via sol — rejected, sol exceptions in the protected-call path surface as generic `plugin error`, losing the stage context.

- **params marshal: `sol::table` → `std::map<std::string,std::string>`; non-string scalar values coerced with `tostring()`; nested tables rejected with an error.** Mangafire passes everything as strings already; coercion keeps the tool forgiving without defining object semantics. Alternative: `std::variant`/JSON object — rejected, YAGNI.

- **`api_path` leading `/api` trimmed inside the host** (exact goIsekai behavior: `strings.TrimPrefix(apiPath, "/api")`) — plugins stay dumb about the rule. Empty/nil `api_path` → error.

- **Test placement: new `tests/test_vrf.cpp` doctest target** (own binary, registered in xmake.lua/CMakeLists alongside existing test targets). Vectors ported from goIsekai `vrf_sign_test.go` plus the empty-params and bad-stage cases from the spec delta. Alternative: extend `test_plugin.cpp` — rejected, plugin tests are about ABI wiring; the transform deserves isolated vectors.

## Risks / Trade-offs

- [Silent algorithm drift vs goIsekai when ported] → port the goIsekai test vectors verbatim and require exact string equality; a table-driven C++ test catches any mismatch before any plugin touches it.
- [Lua number coercion: `iv` arrives as double] → accept integer 0-255 only; reject fractional/out-of-range with `vrf_sign: stage <n>: iv out of range`.
- [Plugin supplies huge tables] → stages are fixed 3-ish entries from the site; no size guard beyond decoded-bytes sanity (errors on length-0 `key`/`tbl`).
- [VRF is effectively security-through-obscurity] → that's inherent to the site's scheme; the commit message and OKF note should not call it "cryptographic" beyond what it is (a signing transform).

## Migration Plan

No deploy concerns — plugin host tool, additive. Host tools are listed to Lua at plugin load; existing plugins ignore unknown globals. Rollback = revert the change; no data written anywhere.

## Open Questions

None.
