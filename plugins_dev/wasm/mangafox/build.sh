#!/bin/sh
# Build the mangafox wasm guest -> main.wasm (next to this file).
# Host ABI imports are documented in main.cpp header; runtime = wasm3 (src/plugin/wasm/).
cd "$(dirname "$0")" || exit 1
exec clang++ --target=wasm32-wasip1 --sysroot=/opt/wasi-sdk/share/wasi-sysroot \
    -std=c++17 -O2 -nostartfiles -nodefaultlibs \
    -Wl,--no-entry -Wl,--allow-undefined \
    -Wl,--export=alloc -Wl,--export=handle -Wl,--export=last_len \
    main.cpp \
    /opt/wasi-sdk/share/wasi-sysroot/lib/wasm32-wasi/libc++.a \
    /opt/wasi-sdk/share/wasi-sysroot/lib/wasm32-wasi/libc++abi.a \
    /opt/wasi-sdk/share/wasi-sysroot/lib/wasm32-wasi/libc.a \
    -o main.wasm
