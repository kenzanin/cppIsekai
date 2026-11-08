// tests/fixtures/plugins/wasmspin/spin.c
// Freestanding wasm fixture for the gas/trap path: handle() recurses without
// bound, so the call ends in a wasm3 trap (gas or stack) that the host maps to
// ok:false. Exports the full ABI so load() accepts it.
//
// Regen:
//   clang --target=wasm32 -nostdlib -O1 -Wl,--no-entry \
//     -Wl,--export=alloc -Wl,--export=handle -Wl,--export=last_len \
//     spin.c -o main.wasm

static unsigned int depth = 0;

unsigned int alloc(unsigned int len) {
    (void)len;
    return 0; // never reached: handle traps before replying
}

unsigned int last_len(void) {
    return 0;
}

unsigned int handle(unsigned int in_ptr, unsigned int in_len) {
    (void)in_ptr;
    (void)in_len;
    if (depth < 200000u) {
        ++depth;
        return handle(in_ptr, in_len);
    }
    return 0;
}
