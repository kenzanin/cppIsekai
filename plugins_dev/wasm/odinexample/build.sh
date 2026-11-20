#!/bin/sh
# Odin wasm guest build. Pin: odin dev-2026-10:84bc3fc21 (see README.md).
set -e
cd "$(dirname "$0")"
odin build . -target:wasi_wasm32 -o:speed
mv -f odinexample.wasm main.wasm
