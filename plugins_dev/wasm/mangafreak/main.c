// Mangafreak wasm plugin (port-multi-runtime 3.3 pilot; port of goIsekai
// lua/mangafreak). Freestanding-ish C on wasi libc; env.* imports are the
// cppIsekai host ABI (see src/plugin/wasm/wasm_imports.cpp kImports):
//   http_fetch_body(url,url_len,opts,opts_len,out,cap) -> len | -1
//   re2_find(pat,pat_len,txt,txt_len,out,cap)          -> len (JSON ["g0",..]) | -1
//   re2_find_all(pat,pat_len,txt,txt_len,out,cap)      -> len (JSON [[..]])   | -1
//   html_attr(html,html_len,sel,sel_len,attr,attr_len,out,cap) -> len | -1
//   text_strip_html(in,len,out,cap) -> len | -1
//   text_chapter_num(in,len) -> i64
//   url_encode(in,len,out,cap) -> len | -1
// Envelope in: {"api":1,"method":"..","params":{..}}; reply out: ok/data JSON.
//
// Build (see memory #5340 / fixture README -- system clang+lld, wasi sysroot):
//   clang --target=wasm32-wasip1 --sysroot=/opt/wasi-sdk/share/wasi-sysroot \
//     -O2 -nostartfiles -nodefaultlibs -Wl,--no-entry -Wl,--allow-undefined \
//     -Wl,--export=alloc -Wl,--export=handle -Wl,--export=last_len \
//     main.c /opt/wasi-sdk/share/wasi-sysroot/lib/wasm32-wasi/libc.a -o main.wasm

#define _GNU_SOURCE // memmem on wasi libc
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#define BASE "https://ww3.mangafreak.me"

__attribute__((import_module("env"), import_name("http_fetch_body")))
int http_fetch_body(unsigned int a, unsigned int b, unsigned int c, unsigned int d, unsigned int o, unsigned int cap);
__attribute__((import_module("env"), import_name("re2_find")))
int re2_find(unsigned int a, unsigned int b, unsigned int c, unsigned int d, unsigned int o, unsigned int cap);
__attribute__((import_module("env"), import_name("re2_find_all")))
int re2_find_all(unsigned int a, unsigned int b, unsigned int c, unsigned int d, unsigned int e, unsigned int f, unsigned int g, unsigned int h, unsigned int i, unsigned int j);
__attribute__((import_module("env"), import_name("html_attr")))
int html_attr(unsigned int a, unsigned int b, unsigned int c, unsigned int d, unsigned int e, unsigned int f, unsigned int o, unsigned int cap);
__attribute__((import_module("env"), import_name("text_strip_html")))
int text_strip_html(unsigned int a, unsigned int b, unsigned int o, unsigned int cap);
__attribute__((import_module("env"), import_name("text_chapter_num")))
long long text_chapter_num(unsigned int a, unsigned int b);
__attribute__((import_module("env"), import_name("url_encode")))
int url_encode(unsigned int a, unsigned int b, unsigned int o, unsigned int cap);

// ── guest memory ────────────────────────────────────────────────────────────
static unsigned char ARENA[1 << 20]; // reply bump arena, reset per handle
static unsigned int used = 0;
static unsigned int last_len_v = 0;
static char BODY[1 << 20];   // page body scratch (1 MiB; detail pages ~650 KB)
static char TMP[1 << 21];    // regex result scratch (2 MiB; find emits full+group, 2x641K esc)
static char TMP2[1 << 16];
static char ENC[1024];

static unsigned int balloc(unsigned int len) {
    len = (len + 3u) & ~3u;
    if (used + len > sizeof ARENA) {
        return 0;
    }
    unsigned int off = used;
    used += len;
    return (unsigned int)(unsigned long)(ARENA + off);
}

static void emit(const char *s, unsigned int n) { // append to reply buffer
    if (used + n + 4u > sizeof ARENA) {
        return;
    }
    memcpy(ARENA + used, s, n);
    used += n;
}
static void emits(const char *s) { emit(s, (unsigned int)strlen(s)); }

// JSON string with minimal escaping (host glaze reads it back)
static void jstr(const char *s, unsigned int n) {
    char *t = TMP2;
    unsigned int cap = sizeof TMP2 - 2u, k = 0;
    t[k++] = '"';
    for (unsigned int i = 0; i < n && k < cap - 6u; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (c == '"' || c == '\\') {
            t[k++] = '\\';
            t[k++] = c;
        } else if (c < 0x20) {
            k += (unsigned int)snprintf(t + k, 8, "\\u%04x", c);
        } else {
            t[k++] = (char)c;
        }
    }
    t[k++] = '"';
    emit(t, k);
}

// fetch URL body into BODY; returns length or 0
static unsigned int fetch_body(const char *url) {
    int n = http_fetch_body((unsigned int)(unsigned long)url, (unsigned int)strlen(url),
                            (unsigned int)(unsigned long)"{}", 2u,
                            (unsigned int)(unsigned long)BODY, sizeof BODY - 1u);
    return n > 0 ? (unsigned int)n : 0;
}

// re2_find; writes JSON array into TMP, returns len or 0 (no match/error)
static int find_raw(const char *pat, const char *txt, unsigned int txt_len) {
    return re2_find((unsigned int)(unsigned long)pat, (unsigned int)strlen(pat),
                    (unsigned int)(unsigned long)txt, txt_len,
                    (unsigned int)(unsigned long)TMP, sizeof TMP - 1u);
}
static unsigned int find(const char *pat, const char *txt, unsigned int txt_len) {
    int n = find_raw(pat, txt, txt_len);
    return n > 0 ? (unsigned int)n : 0;
}
static unsigned int find_n(const char *pat, const char *txt, unsigned int txt_len) {
    int n = re2_find_all((unsigned int)(unsigned long)pat, (unsigned int)strlen(pat),
                          (unsigned int)(unsigned long)txt, txt_len,
                          (unsigned int)(unsigned long)TMP, sizeof TMP - 1u,
                          0u, 0u, 0u, 0u);
    return n > 0 ? (unsigned int)n : 0;
}

// --- tiny read-only JSON helpers for the ENVELOPE and regex results ---------
// (envelope shape is host-fixed; regex results are host-built JSON arrays)

// find "key":"value" at top of a flat object; returns value len, sets *val
static unsigned int jstr_field(const char *json, unsigned int n, const char *key, const char **val) {
    char pat[64];
    unsigned int k = (unsigned int)snprintf(pat, sizeof pat, "\"%s\":\"", key);
    const char *p = NULL;
    for (unsigned int i = 0; i + k <= n; ++i) {
        if (memcmp(json + i, pat, k) == 0) {
            p = json + i + k;
            break;
        }
    }
    if (!p) {
        return 0;
    }
    unsigned int len = 0;
    while (p + len < json + n) {
        if (p[len] == '\\' && p[len + 1]) {
            len += 2;
            continue;
        }
        if (p[len] == '"') {
            break;
        }
        ++len;
    }
    *val = p;
    return len;
}

// copy possibly-escaped JSON string body to dst unescaped; returns len
static unsigned int unesc(const char *s, unsigned int n, char *dst, unsigned int cap) {
    unsigned int k = 0;
    for (unsigned int i = 0; i < n && k < cap - 1u; ++i) {
        if (s[i] == '\\' && i + 1 < n) {
            char c = s[++i];
            dst[k++] = c == 'n' ? '\n' : c == 't' ? '\t' : c == 'r' ? '\r' : c;
        } else {
            dst[k++] = s[i];
        }
    }
    dst[k] = 0;
    return k;
}

// nth element of a host regex-result JSON: string of ["a","b"] or inner array
// text of [[..],..]. Rotates 8 static bufs (<=4 live per expr); unescapes \".
static const char *json_elem(const char *json, unsigned int n, unsigned int idx, unsigned int *len) {
    static char buf[8][4096];
    static unsigned int slot;
    char *out = buf[slot++ & 7u];
    unsigned int depth = 0, seen = 0, i = 0;
    while (i < n) {
        char c = json[i];
        if (c == '[') {
            ++depth;
            ++i;
        } else if (c == ']') {
            if (depth) {
                --depth;
            }
            ++i;
        } else if (c == '"' && (depth == 1 || depth == 2)) { // flat [a,b] or nested [[a,b]]
            if (seen == idx) {
                unsigned int j = i + 1, k = 0;
                while (j < n && k < 4095u) {
                    if (json[j] == '\\' && j + 1 < n) {
                        out[k++] = json[j + 1];
                        j += 2;
                        continue;
                    }
                    if (json[j] == '"') {
                        break;
                    }
                    out[k++] = json[j++];
                }
                out[k] = 0;
                *len = k;
                return out;
            }
            ++seen;
            ++i;
            while (i < n && json[i] != '"') {
                if (json[i] == '\\') {
                    ++i;
                }
                ++i;
            }
            ++i;
        } else {
            ++i;
        }
    }
    return NULL;
}

// ── replies ────────────────────────────────────────────────────────────────
static unsigned int reply_err(const char *msg) {
    char buf[128];
    int n = snprintf(buf, sizeof buf, "{\"ok\":false,\"error\":\"%s\"}", msg);
    emit(buf, (unsigned int)n); // into the arena, like every other reply
    return (unsigned int)(unsigned long)ARENA;
}

// ── search ─────────────────────────────────────────────────────────────────
static const char PAT_BLOCK[] = "(?s)<div\\s+class=\"mangak?a?_search_item\">(.*?)</div>";
static const char PAT_TITLE[] = "(?s)<h[1-6]\\s*>\\s*<a[^>]*href=\"([^\"]+)\"[^>]*>([^<]+)</a>";
static const char PAT_TITLE2[] = "(?s)<a[^>]*href=\"([^\"]+)\"[^>]*>([^<]+)</a>";
static const char PAT_SLUG[] = "/[Mm]anga/([^/?\"]+)";
static const char PAT_COVER[] = "data-src=\"([^\"]+)\"";
static const char PAT_COVER2[] = "<img[^>]*src=\"([^\"]+)\"";

static void search_reply(const char *q, unsigned int qn) {
    if (url_encode((unsigned int)(unsigned long)q, qn,
                   (unsigned int)(unsigned long)ENC, sizeof ENC - 1u) <= 0) {
        ENC[0] = 0;
    }
    char url[512];
    int un = snprintf(url, sizeof url, BASE "/Find/%s", ENC);
    unsigned int body_len = fetch_body(url);
    if (!body_len) {
        reply_err("fetch failed");
        return;
    }
    emits("{\"ok\":true,\"data\":[");
    unsigned int bn = find_n(PAT_BLOCK, BODY, body_len);
    char seen[64][128];
    unsigned int nseen = 0, emitted = 0;
    for (unsigned int bi = 0; bi < 200; ++bi) { // blocks come one call at a time
        unsigned int blen = 0;
        const char *block = json_elem(TMP, bn, bi, &blen); // re-parses per call: fine at this scale
        if (!block) {
            break;
        }
        unsigned int tn = find(PAT_TITLE, block, blen);
        unsigned int t0 = 0, t1 = 0;
        const char *href = json_elem(TMP, tn, 1, &t0);
        const char *title = json_elem(TMP, tn, 2, &t1);
        if (!href) {
            tn = find(PAT_TITLE2, block, blen);
            href = json_elem(TMP, tn, 1, &t0);
            title = json_elem(TMP, tn, 2, &t1);
        }
        if (!href || !title) {
            continue;
        }
        unsigned int sn = find(PAT_SLUG, href, t0);
        unsigned int s0 = 0;
        const char *slug = json_elem(TMP, sn, 1, &s0);
        if (!slug || !s0) {
            continue;
        }
        int dup = 0;
        for (unsigned int i = 0; i < nseen; ++i) {
            if (nseen < 64 && memcmp(seen[i], slug, s0) == 0 && seen[i][s0] == 0) {
                dup = 1;
            }
        }
        if (dup || nseen >= 64) {
            continue;
        }
        memcpy(seen[nseen++], slug, s0 < 128 ? s0 : 127);
        unsigned int t2 = 0;
        unsigned int cn = find(PAT_COVER, block, blen);
        const char *cover = json_elem(TMP, cn, 1, &t2);
        if (!cover) {
            cn = find(PAT_COVER2, block, blen);
            cover = json_elem(TMP, cn, 1, &t2);
        }
        if (emitted++) {
            emit(",", 1);
        }
        emits("{\"id\":");
        jstr(slug, s0);
        emits(",\"title\":");
        jstr(title, t1);
        emits(",\"cover\":");
        if (cover) {
            jstr(cover, t2);
        } else {
            emits("\"\"");
        }
        emit("}", 1);
    }
    emit("]}", 2);
}

// ── detail ─────────────────────────────────────────────────────────────────
static const char PAT_DATA1[] = "(?s)class=\"manga_series_data\">(.*?)\\s*</div>\\s*</div>";
static const char PAT_TITLE_H1[] = "(?s)<h1>(.*?)</h1>";
static const char PAT_IMG_BLOCK[] = "(?s)class=\"manga_series_image\">(.*?)</div>";
static const char PAT_IMG[] = "<img[^>]*src=\"([^\"]*)\"";
static const char PAT_GENRE_BLOCK[] = "(?s)class=\"series_sub_genre_list\">(.*?)</div>";
static const char PAT_GENRE[] = "<a[^>]*>([^<]+)</a>";
static const char PAT_DESC_BLOCK[] = "(?s)class=\"manga_series_description\">(.*?)</div>\\s*</div>";
static const char PAT_P[] = "(?s)<p>(.*?)</p>";

static void strip_into(const char *s, unsigned int n) { // strip_html -> TMP2
    TMP2[0] = 0;
    if (text_strip_html((unsigned int)(unsigned long)s, n,
                        (unsigned int)(unsigned long)TMP2, sizeof TMP2 - 1u) < 0) {
        TMP2[0] = 0;
    }
}

static void detail_reply(const char *id, unsigned int idn) {
    char url[512];
    int un = snprintf(url, sizeof url, BASE "/Manga/%.*s", (int)idn, id);
    (void)un;
    unsigned int body_len = fetch_body(url);
    if (!body_len) {
        reply_err("fetch failed");
        return;
    }
    unsigned int tn = find(PAT_TITLE_H1, BODY, body_len);
    const char *title = json_elem(TMP, tn, 1, &tn);
    char title_buf[512] = "";
    if (title) {
        strip_into(title, tn);
        memcpy(title_buf, TMP2, sizeof title_buf - 1u);
    }
    unsigned int ib = find(PAT_IMG_BLOCK, BODY, body_len);
    const char *img_block = json_elem(TMP, ib, 1, &ib);
    char cover[512] = "";
    if (img_block) {
        unsigned int cn = find(PAT_IMG, img_block, ib);
        const char *c = json_elem(TMP, cn, 1, &cn);
        if (c) {
            unsigned int cn2 = cn < sizeof cover - 1u ? cn : sizeof cover - 1u;
            memcpy(cover, c, cn2);
            cover[cn2] = 0;
        }
    }
    // status: "This is X series" row -> strip suffix; else bare
    char status[64] = "Unknown";
    {
        const char needle[] = "This is ";
        const char *p = memmem(BODY, body_len, needle, sizeof needle - 1u);
        if (p) {
            p += sizeof needle - 1u;
            unsigned int n = 0;
            while (p[n] && p[n] != '<' && n < sizeof status - 8u) {
                ++n;
            }
            while (n && (p[n - 1] == ' ')) {
                --n;
            }
            unsigned int suf = sizeof(" series") - 1u;
            const char sfx[] = " series";
            if (n > suf && memcmp(p + n - suf, sfx, suf) == 0) {
                n -= suf;
            }
            memcpy(status, p, n);
            status[n] = 0;
        }
    }
    unsigned int gb = find(PAT_GENRE_BLOCK, BODY, body_len);
    const char *gb_s = json_elem(TMP, gb, 1, &gb);
    unsigned int db = find(PAT_DESC_BLOCK, BODY, body_len);
    const char *dsec = json_elem(TMP, db, 1, &db);
    char desc[4096] = "";
    if (dsec) {
        unsigned int pn = find(PAT_P, dsec, db);
        const char *p = json_elem(TMP, pn, 1, &pn);
        if (!p) {
            p = dsec;
            pn = db;
        }
        strip_into(p, pn);
        unsigned int dn = strlen(TMP2) < sizeof desc - 1u ? (unsigned int)strlen(TMP2) : sizeof desc - 1u;
        memcpy(desc, TMP2, dn);
        desc[dn] = 0;
    }
    emits("{\"ok\":true,\"data\":{\"id\":");
    jstr(id, idn);
    emits(",\"title\":");
    jstr(title ? title : "", title ? (unsigned int)strlen(title) : 0);
    emits(",\"cover\":\"");
    emit(cover, (unsigned int)strlen(cover));
    emits("\",\"status\":");
    jstr(status, (unsigned int)strlen(status));
    emits(",\"author\":\"\",\"artist\":\"\",\"genres\":[");
    unsigned int gcount = 0;
    if (gb_s) {
        unsigned int gn = find_n(PAT_GENRE, gb_s, gb);
        for (unsigned int gi = 0;; ++gi) {
            unsigned int glen = 0;
            const char *g = json_elem(TMP, gn, gi, &glen);
            if (!g || !glen) {
                break;
            }
            if (g[0] == '<') { // skip raw-anchor duplicates, keep text form
                continue;
            }
            if (gcount++) {
                emit(",", 1);
            }
            jstr(g, glen);
        }
    }
    emits("],\"description\":");
    jstr(desc, (unsigned int)strlen(desc));
    emit("}}", 2);
}

// ── chapters ───────────────────────────────────────────────────────────────
static const char PAT_LIST[] = "(?s)class=\"manga_series_list\">(.*?)</table>";
static const char PAT_CH[] = "<a[^>]*href=\"([^\"]*Read1_[^\"]+)\"[^>]*>([^<]+)</a>";

static void chapters_reply(const char *id, unsigned int idn) {
    char url[512];
    snprintf(url, sizeof url, BASE "/Manga/%.*s", (int)idn, id);
    unsigned int body_len = fetch_body(url);
    if (!body_len) {
        reply_err("fetch failed");
        return;
    }
    struct Ch {
        char id[256];
        long long num;
        char title[128];
        char href[256];
    };
    static struct Ch chs[2048];
    unsigned int n = 0;
    // locate the series-list table directly in BODY (641 KB; no copy)
    const char *block = BODY;
    unsigned int block_len = body_len;
    {
        const char *bs = memmem(BODY, body_len, "manga_series_list", 17);
        if (bs) {
            const char *be = memmem(bs, body_len - (unsigned int)(bs - BODY), "</table>", 8);
            if (be) {
                block = bs;
                block_len = (unsigned int)(be - bs) + 8;
            }
        }
    }
    // find_n result is [[href,title],...]; json_elem flattens to
    // idx = match*2 + group (proven in search_reply).
    unsigned int mn = find_n(PAT_CH, block, block_len);
    // walk [[full,g1,g2],...]: re2_find_all prepends the full match; the
    // capture groups sit at slots 1..n. Raw-capture between quotes.
    {
        const char *p = TMP;
        const char *end = TMP + mn;
        while (p < end && n < 2048) {
            if (*p != '[') {
                ++p;
                continue;
            }
            const char *e = memchr(p, ']', end - p); // inner array end
            if (!e) {
                break;
            }
            // collect up to 3 quoted strings
            const char *S[3], *E[3];
            int np = 0;
            const char *z = p;
            while (np < 3) {
                const char *a = memchr(z, '"', e - z);
                if (!a) {
                    break;
                }
                const char *b2 = 0;
                for (const char *y = a + 1; y < e; ++y) {
                    if (*y == '\\') {
                        ++y;
                        continue;
                    }
                    if (*y == '"') {
                        b2 = y;
                        break;
                    }
                }
                if (!b2) {
                    break;
                }
                S[np] = a + 1;
                E[np] = b2;
                ++np;
                z = b2 + 1;
            }
            if (np >= 3) { // full, href, title
                char href_buf[256], title_buf[128];
                unsigned int hln = unesc(S[1], (unsigned int)(E[1] - S[1]), href_buf, sizeof href_buf - 1u);
                unsigned int tln = unesc(S[2], (unsigned int)(E[2] - S[2]), title_buf, sizeof title_buf - 1u);
                const char *rp = strstr(href_buf, "/Read1_");
                if (rp && tln) {
                    const char *path = rp + 7;
                    unsigned int plen = hln - (unsigned int)(path - href_buf);
                    struct Ch *c = &chs[n];
                    memset(c, 0, sizeof *c);
                    snprintf(c->id, sizeof c->id, "%.*s:%.*s", (int)idn, id, (int)plen, path);
                    memcpy(c->title, title_buf, tln < sizeof c->title - 1u ? tln : sizeof c->title - 1u);
                    long long v = 0; // ch num = trailing digits of path
                    unsigned int e2 = plen;
                    while (e2 > 0 && path[e2 - 1] >= '0' && path[e2 - 1] <= '9') {
                        --e2;
                    }
                    for (unsigned int d3 = e2; d3 < plen; ++d3) {
                        v = v * 10 + (path[d3] - '0');
                    }
                    c->num = v;
                    memcpy(c->href, href_buf, hln < sizeof c->href - 1u ? hln : sizeof c->href - 1u);
                    ++n;
                }
            }
            p = e + 1;
        }
    }
    emits("{\"ok\":true,\"data\":[");
    for (unsigned int i = 0; i < n; ++i) { // reverse: newest first
        struct Ch *c = &chs[n - 1u - i];
        if (i) {
            emit(",", 1);
        }
        emits("{\"id\":");
        jstr(c->id, (unsigned int)strlen(c->id));
        emits(",\"number\":");
        char nb[32];
        snprintf(nb, sizeof nb, "%lld", c->num);
        emit(nb, (unsigned int)strlen(nb));
        emits(",\"title\":");
        jstr(c->title, (unsigned int)strlen(c->title));
        emit("}", 1);
    }
    emit("]}", 2);
}

// ── pages ──────────────────────────────────────────────────────────────────
static const char PAT_GOHERE[] = "<img[^>]*id=\"gohere\"[^>]*src=\"([^\"]*)\"|<img[^>]*src=\"([^\"]*)\"[^>]*id=\"gohere\"";

static void pages_reply(const char *id, unsigned int idn) {
    // id = "SLUG:Read1_SLUG_CH"; url = BASE/Read1_SLUG_CH
    const char *colon = memchr(id, ':', idn);
    const char *path = colon ? colon + 1 : id;
    unsigned int path_len = (unsigned int)(id + idn - path);
    char url[512];
    snprintf(url, sizeof url, BASE "/Read1_%.*s", (int)path_len, path);
    unsigned int body_len = fetch_body(url);
    if (!body_len) {
        reply_err("fetch failed");
        return;
    }
    emits("{\"ok\":true,\"data\":[");
    unsigned int n = 0;
    // pass 1: gohere imgs (attr order varies; two patterns)
    static const char *pats[] = {
        "<img[^>]*id=\"gohere\"[^>]*src=\"([^\"]+)\"",
        "<img[^>]*src=\"([^\"]+)\"[^>]*id=\"gohere\"",
    };
    for (int pi = 0; pi < 2 && n == 0; ++pi) {
        unsigned int mn = find_n(pats[pi], BODY, body_len);
        for (unsigned int mi = 0;; ++mi) {
            unsigned int u0 = 0;
            const char *u = json_elem(TMP, mn, mi * 2u + 1u, &u0); // [full,g1]: skip full
            if (!u) {
                break;
            }
            if (!u0) {
                continue;
            }
            if (n++) {
                emit(",", 1);
            }
            jstr(u, u0);
        }
    }
    // pass 2 fallback: any http img, skip svg/logo/icon
    if (n == 0) {
        unsigned int mn = find_n("<img[^>]*src=\"(https?://[^\"]+)\"", BODY, body_len);
        for (unsigned int mi = 0;; ++mi) {
            unsigned int u0 = 0;
            const char *u = json_elem(TMP, mn, mi * 2u + 1u, &u0); // [full,g1]: skip full
            if (!u) {
                break;
            }
            if (!u0) {
                continue;
            }
            if (strstr(u, ".svg") || strstr(u, "logo") || strstr(u, "icon")) {
                continue;
            }
            if (n++) {
                emit(",", 1);
            }
            jstr(u, u0);
        }
    }
    emit("]}", 2);
    --n; // (kept count honest for the emit logic above)
    (void)n;
}

// ── entry ──────────────────────────────────────────────────────────────────
unsigned int alloc(unsigned int len) {
    (void)len;
    return (unsigned int)(unsigned long)ARENA; // host copies envelope here
}

unsigned int handle(unsigned int in_ptr, unsigned int in_len) {
    (void)in_ptr;
    used = 0; // fresh arena per call; host reads the reply before the next call
    const char *env = (const char *)(unsigned long)0; // envelope was memcpy'd to alloc()'s address
    // NOTE: host memcpy'd the envelope to the alloc() result == ARENA base.
    env = (const char *)(unsigned long)ARENA;
    unsigned int n = in_len;

    char method[16] = "";
    unsigned int ml = 0;
    const char *mv = NULL;
    if ((ml = jstr_field(env, n, "method", &mv))) {
        ml = ml < sizeof method - 1u ? ml : sizeof method - 1u;
        memcpy(method, mv, ml);
        method[ml] = 0;
    }
    char arg[512] = ""; // params.query / params.id (unescaped)
    unsigned int al = 0;
    if ((al = jstr_field(env, n, "query", &mv))) {
        al = unesc(mv, al < 512 ? al : 511, arg, sizeof arg - 1u);
    } else if ((al = jstr_field(env, n, "id", &mv))) {
        al = unesc(mv, al < 512 ? al : 511, arg, sizeof arg - 1u);
    }

    if (strcmp(method, "search") == 0) {
        search_reply(arg, al);
    } else if (strcmp(method, "detail") == 0) {
        detail_reply(arg, al);
    } else if (strcmp(method, "chapters") == 0) {
        chapters_reply(arg, al);
    } else if (strcmp(method, "pages") == 0) {
        pages_reply(arg, al);
    } else if (strcmp(method, "genres") == 0) { // DEBUG channel: body/hits/rx1
        unsigned int bl2 = fetch_body(BASE "/Find/one%20piece");
        unsigned int bn2 = bl2 ? find_n(PAT_BLOCK, BODY, bl2) : 0;
        unsigned int el = 0;
        const char *e0 = bn2 ? json_elem(TMP, bn2, 0, &el) : 0;
        char dbg[256];
        int dn;
        if (e0) {
            unsigned int show = el < 160 ? el : 160;
            char hx[96];
            unsigned int hn = show < 40 ? show : 40;
            for (unsigned int z = 0; z < hn; ++z) {
                snprintf(hx + z * 2, 3, "%02x", (unsigned char)e0[z]);
            }
            dn = snprintf(dbg, sizeof dbg, "{\"ok\":true,\"data\":{\"el\":%u,\"h\":\"%s\"}}", el, hx);
        } else {
            dn = snprintf(dbg, sizeof dbg, "{\"ok\":true,\"data\":{\"bn\":%u,\"el0\":null}}", bn2);
        }
        emits(dbg); // dbg is NUL-terminated
    } else {
        reply_err("unknown method");
    }
    last_len_v = used;
    return (unsigned int)(unsigned long)ARENA;
}

unsigned int last_len(void) {
    return last_len_v;
}
