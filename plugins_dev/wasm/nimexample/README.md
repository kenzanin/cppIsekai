# nimexample — cppIsekai wasm guest in Nim

Minimal template for a **Nim** wasm plugin on the same guest ABI as the C++
guests (`plugins_dev/wasm/*/main.cpp`): the host writes the JSON envelope
`{"api":1,"method":..,"params":{..}}` at `alloc(len)`, calls
`handle(ptr,len)`, and reads the reply — which must parse as
`{"ok":..}` — from the returned pointer with `last_len()` bytes.

## Build

```sh
./build.sh        # -> main.wasm, requires nim + /opt/wasi-sdk
```

## Version pin: Nim 2.4.2

Verified with **Nim Compiler Version 2.4.2** only. Do not bump casually:

- `parseJson()` (std/json) **SIGSEGVs the compiler itself** on
  `--os:standalone --cpu:wasm32` whenever the call is reachable from an
  `exportc` proc (repro: one `exportc` proc calling `parseJson("{}")`).
  Bisected on 2026-11-20; retest per release before relying on it.
- The workaround lives in `main.nim`: the envelope is parsed with
  **`std/parsejson`** (the event lexer under std/json — `p.next()`,
  `p.tok`, `p.str`). Note `next()` never emits `tkColon`/`tkComma`;
  key-vs-value is tracked by hand (after `{` or a `,` in an object, the
  next string is a key).
- `std/json` *construction* (`%*`, `$`) is fine and may be used for replies.

## Toolchain notes (all hit during the first build)

- `--os:standalone` requires `panicoverride.nim` next to the main module.
- Nim only emits objects (`--noLinking:on`); linking stays explicit in
  `build.sh` so the exports and `--allow-undefined` imports match the C++
  guests exactly. Do not let `nim c` link: its clang driver needs libxml2
  and the wasm-ld from the nim bundle misses libxml2 on this machine —
  link with the system clang against the wasi-sdk sysroot instead.
- `-d:nimAllocPagesViaMalloc` wires Nim's allocator to wasi-libc malloc;
  without it `string`/`seq` do not work in the guest.
- `--mm:arc --threads:off`: no GC, no threads — matches the single-threaded
  wasm3 host.

## Guest rules

- Reply buffers must be module-level (`var reply: string` here): the host
  reads linear memory **after** `handle` returns, so procedure locals are
  freed too early.
- Host services are `env.*` imports (`src/plugin/wasm/wasm_imports.cpp`);
  declare with `{.importc: "name", cdecl.}` and uint32 ptr/len pairs.
- Register in `data/plugins/` with a symlink like the other plugins and
  restart the daemon; purge the plugin's `api_cache` rows after changing
  the reply shape.
