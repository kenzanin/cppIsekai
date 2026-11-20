#!/bin/sh
# Build the nimexample wasm guest -> main.wasm (next to this file).
#
# Nim compiles for wasm32-wasip1 against the wasi-sdk sysroot but only emits
# objects (--noLinking): the link step stays explicit so the ABI exports and
# --allow-undefined imports are exactly the ones the C++ guests use
# (plugins_dev/wasm/mangafox/build.sh). -d:nimAllocPagesViaMalloc puts Nim's
# allocator on wasi-libc malloc, so `string`/`newString` work in the guest.
set -e
cd "$(dirname "$0")" || exit 1
SYS=/opt/wasi-sdk/share/wasi-sysroot
CACHE=.nimcache
rm -rf "$CACHE" main.wasm

nim c --os:standalone --cpu:wasm32 --mm:arc --threads:off --noMain -d:danger \
    --cc:clang -d:nimAllocPagesViaMalloc \
    --passC:"--target=wasm32-wasip1 --sysroot=$SYS" \
    --noLinking:on --nimcache:"$CACHE" --out:nimexample main.nim

clang++ --target=wasm32-wasip1 --sysroot="$SYS" -nostartfiles -nodefaultlibs \
    -Wl,--no-entry -Wl,--allow-undefined \
    -Wl,--export=alloc -Wl,--export=handle -Wl,--export=last_len \
    "$CACHE"/*.o "$SYS/lib/wasm32-wasi/libc.a" -o main.wasm

rm -rf "$CACHE"
ls -l main.wasm
