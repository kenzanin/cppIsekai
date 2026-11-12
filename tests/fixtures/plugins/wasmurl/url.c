// tests/fixtures/plugins/wasmurl/url.c
// Wasi wasm fixture for the env.url_* imports (js-wasm-host-parity 1.4):
// handle() runs "a b&c=d%?" through url_encode, feeds the result to
// url_decode, and replies {"ok":true,"data":{"enc":...,"rt":...}}. Real wasi
// libc (memcpy), no crt; the envelope is not parsed.
//
// Regen (imports pinned to env.* by attribute; -nostartfiles keeps crt and
// _start out, --allow-undefined keeps the env imports as imports, and
// -nodefaultlibs + explicit libc.a skips the wasm builtins the host driver
// does not ship for the wasi triple):
//   clang --target=wasm32-wasip1 --sysroot=/opt/wasi-sdk/share/wasi-sysroot \\
//     -O2 -nostartfiles -nodefaultlibs -Wl,--no-entry -Wl,--allow-undefined \\
//     -Wl,--export=alloc -Wl,--export=handle -Wl,--export=last_len \\
//     url.c /opt/wasi-sdk/share/wasi-sysroot/lib/wasm32-wasi/libc.a \\
//     -o main.wasm
//   (system clang/lld against the sdk sysroot: the sdk's bundled wasm-ld
//   wants a libxml2 soname this host no longer ships)

#include <string.h>

__attribute__((import_module("env"), import_name("url_encode")))
int url_encode(unsigned int in_off, unsigned int in_len, unsigned int out, unsigned int cap);

__attribute__((import_module("env"), import_name("url_decode")))
int url_decode(unsigned int in_off, unsigned int in_len, unsigned int out, unsigned int cap);

static const char IN[] = "a b&c=d%?";
static const char P1[] = "{\"ok\":true,\"data\":{\"enc\":\"";
static const char P2[] = "\",\"rt\":\"";
static const char P3[] = "\"}}";

static unsigned char arena[8192];
static unsigned int used = 0;
static char enc_buf[128];
static char rt_buf[128];
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

    const unsigned int enc_off = (unsigned int)(unsigned long)enc_buf;
    const unsigned int rt_off = (unsigned int)(unsigned long)rt_buf;
    const int enc_len = url_encode((unsigned int)(unsigned long)IN, sizeof IN - 1u, enc_off, (unsigned int)sizeof enc_buf);
    if (enc_len < 0) {
        return 0;
    }
    const int rt_len = url_decode(enc_off, (unsigned int)enc_len, rt_off, (unsigned int)sizeof rt_buf);
    if (rt_len < 0) {
        return 0;
    }

    const unsigned int p1_len = (unsigned int)(sizeof P1 - 1u);
    const unsigned int p2_len = (unsigned int)(sizeof P2 - 1u);
    const unsigned int p3_len = (unsigned int)(sizeof P3 - 1u);
    const unsigned int total = p1_len + (unsigned int)enc_len + p2_len + (unsigned int)rt_len + p3_len;
    const unsigned int off = used;
    if (balloc(total) == 0) {
        return 0;
    }
    unsigned int at = off;
    memcpy(arena + at, P1, p1_len);
    at += p1_len;
    memcpy(arena + at, enc_buf, (unsigned int)enc_len);
    at += (unsigned int)enc_len;
    memcpy(arena + at, P2, p2_len);
    at += p2_len;
    memcpy(arena + at, rt_buf, (unsigned int)rt_len);
    at += (unsigned int)rt_len;
    memcpy(arena + at, P3, p3_len);

    last_len_v = total;
    return (unsigned int)(unsigned long)(arena + off);
}

unsigned int last_len(void) {
    return last_len_v;
}
