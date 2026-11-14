#!/bin/sh
# Build the mangafreak wasm guest -> main.wasm (next to this file).
# Host ABI imports are documented in main.c header; runtime = wasm3 (src/plugin/wasm/).
cd "$(dirname "$0")" || exit 1
exec clang --target=wasm32-wasip1 --sysroot=/opt/wasi-sdk/share/wasi-sysroot \
    -O2 -nostartfiles -nodefaultlibs \
    -Wl,--no-entry -Wl,--allow-undefined \
    -Wl,--export=alloc -Wl,--export=handle -Wl,--export=last_len \
    main.c /opt/wasi-sdk/share/wasi-sysroot/lib/wasm32-wasi/libc.a -o main.wasm
