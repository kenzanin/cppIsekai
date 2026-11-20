#!/bin/sh
# Build the vexample wasm guest -> main.wasm (next to this file).
#
# V's WebAssembly backend (vlib/v/gen/wasm) emits the module directly -- no
# C compiler, no wasi-sdk, no binaryen. `pub fn` becomes an undecorated
# export, `@[wasm_import_namespace: 'env']` declares the host imports, and
# an empty main() keeps the WASI _start import-free. Globals need
# -enable-globals; -prod runs the SSA optimizer.
set -e
cd "$(dirname "$0")" || exit 1
V=${V:-v}
command -v "$V" >/dev/null 2>&1 || V=/home/kenzanin/build/vlang/v
rm -f main.wasm
"$V" -enable-globals -b wasm -os wasi -prod -o main.wasm main.v
ls -l main.wasm
