# wasm fixture

- `fixture.c` -> `main.wasm`: freestanding module exporting `alloc`/`handle`/`last_len`
  (host ABI, change plugin-js-wasm design D3). Regenerate:

  ```bash
  clang --target=wasm32 -nostdlib -O1 -Wl,--no-entry \
    -Wl,--export=alloc -Wl,--export=handle -Wl,--export=last_len \
    fixture.c -o main.wasm
  ```

- `../wasmbad/main.wasm` is intentionally non-wasm bytes for the load-failure test.
