# vexample — cppIsekai wasm guest in V

Minimal template guest for the plugin ABI (`src/plugin/wasm/wasm_rt.hpp`):
host writes the JSON envelope at `alloc(len)`, calls `handle(ptr, len)`, reads
the reply (`last_len()` bytes) from the returned pointer. Demonstrates the
`env.text_trim` / `env.url_encode` host imports.

Verified end to end against the running daemon (search / detail / unknown
method / empty-after-trim), plus `hostcheck.mjs` as a node-side pre-check.

## Build

```sh
./build.sh    # v -enable-globals -b wasm -os wasi -prod; no C toolchain
```

Register by symlinking (same as every wasm guest):

```sh
ln -sfn ../../plugins_dev/wasm/vexample ../../data/plugins/vexample
```
(from this dir; or absolute paths from the repo root)

## V wasm-backend traps (V 2.4.x, `vlib/v/gen/wasm`, SSA backend)

Paid for by probe bisection — recheck after any V upgrade:

1. **`__global` arrays collapse onto address 0.** Every global array gets a
   hidden pointer slot and *every slot is emitted zero-initialized*, so two
   arrays occupy the same memory (`gb[5]=1; ga[5]=2` reads back `2`). Use ONE
   `__global` array plus offset consts for all regions (this file does).
   Scalar globals (`int`/`u32`) are fine.
2. **`&arr` vs `&arr[0]`.** `&arr` is the hidden slot address (useless for
   ABI); `&arr[0]` lowers to *load the data pointer from the slot* and is the
   real base — the only valid value to return to the host.
3. **`a[i] = b[j]` across two globals drops the store.** Route through a
   local: `t := b[j]; a[i] = t`.
4. **A zeroing loop directly before a copy loop into the same array drops
   the copy's stores** (any index vars). Write a terminator (`arr[n] = 0`)
   instead of pre-zeroing.
5. **Strings are single-quoted** — backticks are runes. Const names must be
   lower_snake_case. Pointer indexing needs `unsafe`.
6. Compile via the full path if `v` is not on PATH
   (`/home/kenzanin/build/vlang/v` here).
7. `-prod` optional; `-enable-globals` required for any `__global`. Empty
   `main()` keeps the module import-free (the host only resolves named
   exports, `_start` is never run to completion semantics-wise).

`hostcheck.mjs` is the fast lane: `node hostcheck.mjs` instantiates
`main.wasm` with node stubs for the two host imports and asserts the four
envelope shapes before the daemon gate. The daemon restart + purge of the
plugin's `api_cache` rows is still the real gate (`#5426`).
