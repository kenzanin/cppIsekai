// Komix.to wasm guest — C++17, wasi libc++. Port of goIsekai
// examples/plugins/wasm/komix_to_cpp onto the cppIsekai wasm3 ABI
// (src/plugin/wasm/wasm_imports.cpp): envelope {"api":1,"method":..,"params":..}
// in, {"ok":..,"data":..} out via returned ptr + last_len(). Class state hides
// behind function-local statics: lazy init, survives a host that never runs
// __wasm_call_ctors.
// Site facts (from the goIsekai port, 2026-11):
//   search   /api/page-data/browse?q=&page=N -> {"results":[{slug,title,
//            cover_image,status,..}],"totalCount":N}; 60 rows/page
//   genre    /genres/<slug> -> /title/<slug> links
//   detail   /title/<slug> -> og:title/-image/-description, /genres/* links
//   chapters /title/<slug> embedded "chapters":[{chapter_number,title,..}],
//            possibly quote-escaped
//   pages    /title/<slug>/chapter/<n> embedded "pages":[{image_url,enc}];
//            empty image_url -> /api/images?t=<enc>
// IDs: manga slug; chapter "<slug>:<num>". Build: ./build.sh
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <vector>

extern "C" int http_fetch_body(unsigned, unsigned, unsigned, unsigned, unsigned, unsigned);
extern "C" int re2_find_all(unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned,
                            unsigned, unsigned);
extern "C" int url_encode(unsigned, unsigned, unsigned, unsigned);
extern "C" int text_unescape(unsigned, unsigned, unsigned, unsigned);
extern "C" int text_normalize_status(unsigned, unsigned, unsigned, unsigned);

static std::string &body_buf() { static std::string s; return s; }  // last fetched page
static std::string &rb1() { static std::string s; return s; }       // regex json, nesting level 0
static std::string &rb2() { static std::string s; return s; }       // regex json, nested level
static std::string &reply() { static std::string s; return s; }
static std::string &inbuf() { static std::string s; return s; }
static unsigned g_last_len = 0;

static const char BASE[] = "https://komix.to";
static const char API_OPTS[] = R"({"headers":{"Accept":"application/json","Referer":"https://komix.to/"}})";

static bool fetch(const std::string &url, const char *opts) {
    std::string &b = body_buf();
    b.resize(1 << 21);
    int n = http_fetch_body((unsigned)(size_t)url.data(), (unsigned)url.size(), (unsigned)(size_t)opts,
                            (unsigned)strlen(opts), (unsigned)(size_t)b.data(), b.size() - 1);
    if (n <= 0)
        return false;
    b.resize((size_t)n);
    return true;
}

// host re2 global find: rows of capture groups, row[i][0] = whole match
static std::vector<std::vector<std::string>> rx(const char *pat, const std::string &txt, std::string &buf) {
    std::vector<std::vector<std::string>> rows;
    buf.resize(1 << 20);
    int n = re2_find_all((unsigned)(size_t)pat, strlen(pat), (unsigned)(size_t)txt.data(), (unsigned)txt.size(),
                         (unsigned)(size_t)buf.data(), buf.size() - 1, 0, 0, 0, 0);
    if (n <= 0)
        return rows;
    const char *s = buf.data();
    const size_t len = (size_t)n;
    int depth = 0;
    std::vector<std::string> cur;
    for (size_t i = 0; i < len; ++i) {
        char c = s[i];
        if (c == '[') {
            ++depth;
            if (depth == 2) cur.clear();
        } else if (c == ']') {
            if (depth == 2) rows.push_back(cur);
            if (depth) --depth;
        } else if (c == '"') {
            std::string v;
            ++i;
            while (i < len && s[i] != '"') {
                if (s[i] == '\\' && i + 1 < len) {
                    char e = s[++i];
                    v += e == 'n' ? '\n' : e == 't' ? '\t' : e == 'r' ? '\r' : e;
                } else {
                    v += s[i];
                }
                ++i;
            }
            if (depth == 2) cur.push_back(v);
        }
    }
    return rows;
}

static std::string env_field(const char *key) {
    const std::string pat = std::string("\"") + key + "\":\"";
    size_t p = inbuf().find(pat);
    if (p == std::string::npos)
        return "";
    p += pat.size();
    std::string v;
    while (p < inbuf().size() && inbuf()[p] != '"') {
        if (inbuf()[p] == '\\' && p + 1 < inbuf().size()) {
            char e = inbuf()[++p];
            v += e == 'n' ? '\n' : e == 't' ? '\t' : e;
        } else {
            v += inbuf()[p];
        }
        ++p;
    }
    return v;
}

static void emit(const char *s) { reply() += s; }

static void jemit(const std::string &v) {
    reply() += '"';
    for (char c : v) {
        unsigned char u = (unsigned char)c;
        if (c == '"' || c == '\\') {
            reply() += '\\';
            reply() += c;
        } else if (u < 0x20) {
            char b[8];
            snprintf(b, sizeof b, "\\u%04x", u);
            reply() += b;
        } else {
            reply() += c;
        }
    }
    reply() += '"';
}

static void reply_err(const char *m) { reply() = std::string("{\"ok\":false,\"error\":\"") + m + "\"}"; }

// ── text helpers (ported from the goIsekai util.h) ───────────────────────
static std::string trim(const std::string &s) {
    const size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    const size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

// "some-slug" -> "Some Slug"
static std::string humanize(const std::string &slug) {
    std::string out;
    bool cap = true;
    for (char c : slug) {
        if (c == '-' || c == '_') { out += ' '; cap = true; }
        else if (cap && c >= 'a' && c <= 'z') { out += (char)(c - 32); cap = false; }
        else { out += c; cap = false; }
    }
    return out;
}

// "Sci Fi" -> "sci-fi"
static std::string genre_slug(const std::string &g) {
    std::string s = trim(g);
    for (char &c : s) c = c == ' ' ? '-' : (char)tolower((unsigned char)c);
    return s;
}

// host status vocabulary; unknown input passes through
static std::string norm_status(const std::string &s) {
    if (s.empty()) return "";
    std::string sb(64, '\0');
    int n = text_normalize_status((unsigned)(size_t)s.data(), (unsigned)s.size(), (unsigned)(size_t)sb.data(),
                                  sb.size() - 1);
    return n > 0 ? sb.substr(0, (size_t)n) : std::string();
}

// resolve a possibly-relative URL against the site origin
static std::string make_abs(const char *origin, const std::string &raw) {
    std::string u = trim(raw);
    if (u.empty()) return "";
    if (u.compare(0, 2, "//") == 0) return std::string("https:") + u;
    if (u.rfind("http://", 0) == 0 || u.rfind("https://", 0) == 0) return u;
    return u[0] == '/' ? std::string(origin) + u : std::string(origin) + "/" + u;
}

// JSON string-interior unescape, for values captured out of page/API JSON.
// \uXXXX becomes UTF-8 (pairs included) so URLs and titles survive intact.
static std::string junesc(const std::string &s) {
    auto hex = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    auto hex4 = [&hex](const std::string &t, size_t p, unsigned &out) {
        if (p + 4 > t.size()) return false;
        unsigned v = 0;
        for (int k = 0; k < 4; ++k) {
            const int d = hex(t[p + (size_t)k]);
            if (d < 0) return false;
            v = v * 16 + (unsigned)d;
        }
        out = v;
        return true;
    };
    std::string o;
    o.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];
        if (c != '\\' || i + 1 >= s.size()) {
            o += c;
            continue;
        }
        const char e = s[++i];
        switch (e) {
        case 'n': o += '\n'; break;
        case 't': o += '\t'; break;
        case 'r': o += '\r'; break;
        case 'b': o += '\b'; break;
        case 'f': o += '\f'; break;
        case 'u': {
            unsigned cp = 0;
            if (!hex4(s, i + 1, cp)) { o += 'u'; break; }
            i += 4;
            if (cp >= 0xD800 && cp <= 0xDBFF && i + 6 < s.size() && s[i + 1] == '\\' && s[i + 2] == 'u') {
                unsigned lo = 0;
                if (hex4(s, i + 3, lo) && lo >= 0xDC00 && lo <= 0xDFFF) {
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    i += 6;
                }
            }
            if (cp < 0x80) {
                o += (char)cp;
            } else if (cp < 0x800) {
                o += (char)(0xC0 | (cp >> 6));
                o += (char)(0x80 | (cp & 0x3F));
            } else {
                o += (char)(0xE0 | (cp >> 12));
                o += (char)(0x80 | ((cp >> 6) & 0x3F));
                o += (char)(0x80 | (cp & 0x3F));
            }
            break;
        }
        default: o += e;  // \" \\ \/ and pass-through
        }
    }
    return o;
}

// value of "key" in a flat JSON object chunk, decoded; "" when absent
static std::string jfield(const std::string &obj, const char *key) {
    size_t p = obj.find(std::string("\"") + key + "\":");
    if (p == std::string::npos) return "";
    p += strlen(key) + 3;
    while (p < obj.size() && isspace((unsigned char)obj[p])) ++p;
    if (p >= obj.size() || obj[p] != '"') return "";
    ++p;
    std::string raw;
    while (p < obj.size() && obj[p] != '"') {
        if (obj[p] == '\\' && p + 1 < obj.size()) {
            raw += obj[p];
            raw += obj[p + 1];
            p += 2;
        } else {
            raw += obj[p++];
        }
    }
    return junesc(raw);
}

// bare number token after "key":, "" when absent
static std::string jnum(const std::string &obj, const char *key) {
    size_t p = obj.find(std::string("\"") + key + "\":");
    if (p == std::string::npos) return "";
    p += strlen(key) + 3;
    while (p < obj.size() && isspace((unsigned char)obj[p])) ++p;
    std::string v;
    while (p < obj.size() && (isdigit((unsigned char)obj[p]) || strchr(".eE+-", obj[p]))) v += obj[p++];
    return v;
}

// Next.js embeds payloads as escaped JSON: turn \" back into " (as the
// goIsekai port did before parsing).
static void unesc_quotes(std::string &s) {
    std::string o;
    o.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size() && s[i + 1] == '"') {
            o += '"';
            ++i;
        } else {
            o += s[i];
        }
    }
    s.swap(o);
}

// ── search ───────────────────────────────────────────────────────────────
static void genre_listing(const std::string &genre);

static void search_reply(const std::string &query, const std::string &genre) {
    if (trim(query).empty()) {
        if (!trim(genre).empty()) return genre_listing(genre);
        return emit(R"({"ok":true,"data":{"items":[],"total":0}})");
    }
    char enc[512] = "";
    int n = url_encode((unsigned)(size_t)query.data(), (unsigned)query.size(), (unsigned)(size_t)enc,
                       sizeof enc - 1);
    if (n > 0)
        enc[n] = 0;
    // ponytail: 5-page cap (~300 titles); raise when queries must reach deeper,
    // or stop early once the page budget is spent.
    const int kMaxSearchPages = 5;
    long total = -1;
    size_t count = 0;
    std::set<std::string> seen;
    emit(R"({"ok":true,"data":{"items":[)");
    for (int page = 1; page <= kMaxSearchPages; ++page) {
        if (!fetch(std::string(BASE) + "/api/page-data/browse?q=" + enc + "&page=" + std::to_string(page),
                   API_OPTS))
            break;
        int added = 0;
        for (const auto &row : rx(R"(\{[^{}]*\})", body_buf(), rb1())) {
            const std::string &obj = row[0];
            const std::string slug = trim(jfield(obj, "slug"));
            if (slug.empty() || !seen.insert(slug).second) continue;
            if (count) emit(",");
            emit(R"({"id":)");
            jemit(slug);
            emit(R"(,"title":)");
            jemit(jfield(obj, "title"));
            emit(R"(,"cover":)");
            jemit(make_abs(BASE, jfield(obj, "cover_image")));
            emit(R"(,"status":)");
            jemit(norm_status(jfield(obj, "status")));
            emit("}");
            ++count;
            ++added;
        }
        if (added == 0) break;  // past the last page
        if (total < 0) {
            const auto t = rx(R"("totalCount":\s*([0-9]+))", body_buf(), rb2());
            if (!t.empty() && t[0].size() > 1) total = atol(t[0][1].c_str());
        }
        if (total > 0 && (long)count >= total) break;  // everything collected
    }
    char tail[32];
    snprintf(tail, sizeof tail, "],\"total\":%zu}}", total > 0 ? (size_t)total : count);
    emit(tail);
}

static void genre_listing(const std::string &genre) {
    char enc[128] = "";
    const std::string gs = genre_slug(genre);
    int n = url_encode((unsigned)(size_t)gs.data(), (unsigned)gs.size(), (unsigned)(size_t)enc, sizeof enc - 1);
    if (n > 0)
        enc[n] = 0;
    size_t count = 0;
    std::set<std::string> seen;
    emit(R"({"ok":true,"data":{"items":[)");
    if (fetch(std::string(BASE) + "/genres/" + enc, "{}")) {
        const std::string want = humanize(gs);
        for (const auto &r : rx(R"(/title/([a-z0-9-]+))", body_buf(), rb1())) {
            if (count >= 48 || r.size() < 2) continue;
            if (!seen.insert(r[1]).second) continue;
            if (count) emit(",");
            emit(R"({"id":)");
            jemit(r[1]);
            emit(R"(,"title":)");
            jemit(humanize(r[1]));
            emit(R"(,"cover":"","genres":[)");
            jemit(want);
            emit("]}");
            ++count;
        }
    }
    char tail[32];
    snprintf(tail, sizeof tail, "],\"total\":%zu}}", count);
    emit(tail);
}

// ── detail ───────────────────────────────────────────────────────────────
static void detail_reply(const std::string &slug) {
    char enc[128] = "";
    int n = url_encode((unsigned)(size_t)slug.data(), (unsigned)slug.size(), (unsigned)(size_t)enc,
                       sizeof enc - 1);
    if (n > 0)
        enc[n] = 0;
    if (!fetch(std::string(BASE) + "/title/" + enc, "{}"))
        return reply_err("fetch failed");
    auto one = [&](const char *pat) -> std::string {
        const auto r = rx(pat, body_buf(), rb1());
        return r.size() && r[0].size() > 1 ? r[0][1] : std::string();
    };
    std::string title = one("og:title\"\\s*content=\"([^\"]+)\"");
    const char suffix[] = " - Read Free Online | Komix";
    const size_t sl = strlen(suffix);
    if (title.size() >= sl && title.compare(title.size() - sl, sl, suffix) == 0)
        title = title.substr(0, title.size() - sl);
    if (title.empty()) title = humanize(slug);
    const std::string cover = one("og:image\"\\s*content=\"([^\"]+)\"");
    std::string desc;
    const std::string d = one("og:description\"\\s*content=\"([^\"]+)\"");
    if (!d.empty()) {
        std::string sb(1 << 15, '\0');
        const int dn = text_unescape((unsigned)(size_t)d.data(), (unsigned)d.size(), (unsigned)(size_t)sb.data(),
                                     sb.size() - 1);
        if (dn > 0) desc = sb.substr(0, (size_t)dn);
    }
    std::set<std::string> seen;
    std::string tags = "[";
    for (const auto &g : rx(R"(/genres/([a-z0-9-]+))", body_buf(), rb2())) {
        const std::string name = humanize(g.size() > 1 ? g[1] : std::string());
        if (name.empty() || !seen.insert(name).second) continue;
        if (tags.size() > 1) tags += ',';
        tags += '"';
        tags += name;
        tags += '"';
    }
    tags += ']';
    emit(R"({"ok":true,"data":{"id":)");
    jemit(slug);
    emit(R"(,"name":)");
    jemit(title);
    emit(R"(,"cover":)");
    jemit(cover);
    emit(R"(,"status":"","release_at":0,"authors":[],"artists":[],"tags":)");
    emit(tags.c_str());
    emit(R"(,"description":)");
    jemit(desc);
    emit("}}");
}

// ── chapters ─────────────────────────────────────────────────────────────
static void chapters_reply(const std::string &slug) {
    char enc[128] = "";
    int n = url_encode((unsigned)(size_t)slug.data(), (unsigned)slug.size(), (unsigned)(size_t)enc,
                       sizeof enc - 1);
    if (n > 0)
        enc[n] = 0;
    if (!fetch(std::string(BASE) + "/title/" + enc, "{}"))
        return reply_err("fetch failed");
    const auto m = rx(R"(\\?"chapters\\?":\s*\[(.*?)\])", body_buf(), rb1());
    std::string chunk = m.empty() || m[0].size() < 2 ? std::string() : m[0][1];
    unesc_quotes(chunk);
    emit(R"({"ok":true,"data":[)");
    size_t count = 0;
    if (!chunk.empty()) {
        for (const auto &row : rx(R"(\{[^{}]*\})", chunk, rb2())) {
            const std::string &obj = row[0];
            std::string num = jnum(obj, "chapter_number");
            if (num.empty()) num = "0";
            if (count) emit(",");
            emit(R"({"id":)");
            jemit(slug + ":" + num);
            emit(R"(,"number":)");
            emit(num.c_str());
            emit(R"(,"title":)");
            jemit(jfield(obj, "title"));
            emit("}");
            ++count;
        }
    }
    emit("]}");
}

// ── pages ────────────────────────────────────────────────────────────────
static void pages_reply(const std::string &id) {
    std::string page_url;
    const size_t colon = id.find(':');
    if (colon == std::string::npos) {
        page_url = id;
    } else {
        char e1[128] = "", e2[64] = "";
        const std::string slug = id.substr(0, colon), num = id.substr(colon + 1);
        int n1 = url_encode((unsigned)(size_t)slug.data(), (unsigned)slug.size(), (unsigned)(size_t)e1,
                            sizeof e1 - 1);
        int n2 = url_encode((unsigned)(size_t)num.data(), (unsigned)num.size(), (unsigned)(size_t)e2,
                            sizeof e2 - 1);
        if (n1 > 0) e1[n1] = 0;
        if (n2 > 0) e2[n2] = 0;
        page_url = std::string(BASE) + "/title/" + e1 + "/chapter/" + e2;
    }
    if (!fetch(page_url, "{}"))
        return reply_err("fetch failed");
    std::string raw = body_buf();
    unesc_quotes(raw);
    const auto m = rx(R"("pages":\s*\[(.*?)\])", raw, rb1());
    std::string chunk = m.empty() || m[0].size() < 2 ? std::string() : m[0][1];
    emit(R"({"ok":true,"data":[)");
    size_t count = 0;
    if (!chunk.empty()) {
        for (const auto &row : rx(R"(\{[^{}]*\})", chunk, rb2())) {
            const std::string &obj = row[0];
            std::string u = jfield(obj, "image_url");
            const std::string enc = jfield(obj, "enc");
            if (u.empty() && !enc.empty()) u = std::string(BASE) + "/api/images?t=" + enc;
            if (u.empty()) continue;
            if (count) emit(",");
            jemit(u);
            ++count;
        }
    }
    emit("]}");
}

// ── genres ───────────────────────────────────────────────────────────────
static void genres_reply() {
    std::vector<std::string> names = {"Action", "Adventure", "Comedy", "Drama", "Fantasy", "Isekai", "Romance"};
    std::string body;
    if (fetch(std::string(BASE) + "/genres", "{}")) {
        std::set<std::string> seen;
        names.clear();
        for (const auto &g : rx(R"(/genres/([a-z0-9-]+))", body_buf(), rb1())) {
            const std::string name = humanize(g.size() > 1 ? g[1] : std::string());
            if (name.empty() || !seen.insert(name).second) continue;
            names.push_back(name);
        }
    }
    emit(R"({"ok":true,"data":[)");
    for (size_t i = 0; i < names.size(); ++i) {
        if (i) emit(",");
        jemit(names[i]); // plain strings: the chip renderer prints them raw
    }
    emit("]}");
}

// ── entry ────────────────────────────────────────────────────────────────
extern "C" void __wasm_call_ctors(void);  // lld-generated; wasm3 host runs no start fn

extern "C" unsigned alloc(unsigned len) {
    static bool inited = false;
    if (!inited) {  // ponytail: host ABI has no init hook; ctors carry crt/table init
        __wasm_call_ctors();
        inited = true;
    }
    inbuf().resize(len ? len : 4096);
    return (unsigned)(size_t)inbuf().data();
}

extern "C" unsigned handle(unsigned, unsigned) {
    reply().clear();
    const std::string method = env_field("method");
    if (method == "search") {
        std::string q = env_field("query");
        if (q.empty()) q = env_field("q");
        if (q.empty()) q = env_field("title");
        std::string g = env_field("genre");
        if (g.empty()) g = env_field("genres");
        if (g.empty()) g = env_field("tags"); // UI sends comma-joined chip names
        size_t comma = g.find(',');
        if (comma != std::string::npos) g.resize(comma); // one genre per listing
        search_reply(q, g);
    } else if (method == "detail") {
        detail_reply(env_field("id"));
    } else if (method == "chapters") {
        chapters_reply(env_field("id"));
    } else if (method == "pages") {
        pages_reply(env_field("id"));
    } else if (method == "genres") {
        genres_reply();
    } else {
        reply_err("unknown method");
    }
    g_last_len = (unsigned)reply().size();
    return (unsigned)(size_t)reply().data();
}

extern "C" unsigned last_len() { return g_last_len; }
