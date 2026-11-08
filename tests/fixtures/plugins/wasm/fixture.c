// tests/fixtures/plugins/wasm/fixture.c
// Freestanding wasm fixture for the host ABI (design D3): exports
// alloc/handle/last_len, no imports, no libc. handle replies with a constant
// ok-JSON copied into the bump arena; envelope bytes are not parsed.
//
// Regen:
//   clang --target=wasm32 -nostdlib -O1 -Wl,--no-entry \
//     -Wl,--export=alloc -Wl,--export=handle -Wl,--export=last_len \
//     fixture.c -o main.wasm

static const char REPLY[] = "{\"ok\":true,\"data\":{\"wasm\":\"pong\"}}";
static const unsigned int REPLY_LEN = sizeof(REPLY) - 1;

static unsigned char arena[8192];
static unsigned int used = 0;
static unsigned int last_len_v = 0;

static unsigned int balloc(unsigned int len) {
    const unsigned int off = used;
    used += (len + 3u) & ~3u;
    if (used > (unsigned int)sizeof arena) {
        return 0;
    }
    return (unsigned int)(unsigned long)(arena + off);
}

unsigned int alloc(unsigned int len) {
    return balloc(len);
}

unsigned int handle(unsigned int in_ptr, unsigned int in_len) {
    (void)in_ptr;
    (void)in_len; // ponytail: envelope is not parsed by the fixture
    const unsigned int off = used;
    const unsigned int addr = balloc(REPLY_LEN);
    if (addr == 0) {
        return 0;
    }
    for (unsigned int i = 0; i < REPLY_LEN; ++i) {
        arena[off + i] = (unsigned char)REPLY[i];
    }
    last_len_v = REPLY_LEN;
    return addr;
}

unsigned int last_len(void) {
    return last_len_v;
}
