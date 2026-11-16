#!/bin/sh
# Build the komix_to wasm guest -> main.wasm (next to this file).
# Host ABI imports are documented in main.cpp header; runtime = wasm3 (src/plugin/wasm/).
cd "$(dirname "$0")" || exit 1

# wasm-ld links libxml2.so.2; distros shipping only libxml2.so.16 get a
# user-space shim (no root), as in the goIsekai toolchain notes.
if ldd /opt/wasi-sdk/bin/wasm-ld 2>/dev/null | grep -q 'libxml2.so.2 => not found'; then
    SHIM="${TMPDIR:-/tmp}/wasi-libs"
    mkdir -p "$SHIM"
    [ -e "$SHIM/libxml2.so.2" ] || ln -sf "$(ldconfig -p | awk '/libxml2\.so /{print $NF; exit}')" "$SHIM/libxml2.so.2"
    LD_LIBRARY_PATH="$SHIM${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    export LD_LIBRARY_PATH
fi

exec clang++ --target=wasm32-wasip1 --sysroot=/opt/wasi-sdk/share/wasi-sysroot \
    -std=c++17 -O2 -nostartfiles -nodefaultlibs \
    -Wl,--no-entry -Wl,--allow-undefined \
    -Wl,--export=alloc -Wl,--export=handle -Wl,--export=last_len \
    main.cpp \
    /opt/wasi-sdk/share/wasi-sysroot/lib/wasm32-wasi/libc++.a \
    /opt/wasi-sdk/share/wasi-sysroot/lib/wasm32-wasi/libc++abi.a \
    /opt/wasi-sdk/share/wasi-sysroot/lib/wasm32-wasi/libc.a \
    -o main.wasm
