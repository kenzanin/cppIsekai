# Spec Delta

## ADDED Requirements

### Requirement: Crypto host tool `crypto.vrf_sign`
Plugins SHALL call `crypto.vrf_sign(api_path, params, stages)` to sign an upstream request, mangafire-style. The host SHALL trim a leading `/api` from the sign string, serialize params sorted by key as raw `k=v` pairs, and apply stages in order as `out[i] = tbl[(d ^ key[i % len] ^ prev) & 0xFF]`, `prev` seeded to `iv`. The result SHALL be RawURL base64url without padding. Stages (`iv`, standard-base64 `key`/`tbl`) are plugin-declared. Invalid stage data SHALL fail without a crash.

#### Scenario: Deterministic sign of a known request
- **WHEN** the plugin calls `crypto.vrf_sign("/titles", {limit="50"}, VRF_STAGES)` with a fixed table set
- **THEN** the result is exactly the base64url signature captured in the change's test vectors, and repeated calls return the same string

#### Scenario: Empty params edge
- **WHEN** the plugin passes an empty params table
- **THEN** the signature input omits the `?` separator entirely and the result is the signature of the bare path

#### Scenario: Invalid stage rejected cleanly
- **WHEN** a stage's `key` or `tbl` is not valid standard base64
- **THEN** the call fails with a `vrf_sign: stage <n>: ...` reason, the plugin receives a detectable failure, and the host logs the reason tagged with the plugin id

#### Scenario: Tables stay plugin-side
- **WHEN** a plugin rebuilds its table set after a site rotation
- **THEN** only the plugin file changes; the host binary is not recompiled
