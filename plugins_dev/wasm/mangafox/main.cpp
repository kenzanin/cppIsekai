// MangaFox (fanfox.net) wasm guest — C++17, wasi libc++.
// Host ABI: src/plugin/wasm/wasm_imports.cpp (imports below); envelope
// {"api":1,"method":..,"params":{..}} in, {"ok":..,"data":..} out; reply via
// returned ptr + last_len(). Class state hides behind function-local statics:
// lazy init, survives a host that never runs __wasm_call_ctors.
// Site facts (har-verified 2026-11-14):
//  search   /search?title= -> <a href="/manga/slug/" title="T"> + manga-list-4-cover img
//  detail   /manga/slug/   -> detail-info-right-* blocks, og:image cover
//  chapters m.fanfox.net/manga/slug/ -> /cNNN[.M]/1.html anchors
//  pages    /manga/slug/cNNN/1.html -> chapterid,imagecount; chapterfun.ashx
//           (Referer opt required) -> packed dm5imagefun: pix ends
//           ".../NNN./compressed" (truncated decimal — re-append suffix from
//           cNNN: "0", or the M of cNNN.M); pvalue[] = 2 urls per call.
// Build: ./build.sh
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <set>

extern "C" int http_fetch_body(unsigned, unsigned, unsigned, unsigned, unsigned, unsigned);
extern "C" int re2_find_all(unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned,
                            unsigned, unsigned);
extern "C" int url_encode(unsigned, unsigned, unsigned, unsigned);
extern "C" int text_strip_html(unsigned, unsigned, unsigned, unsigned);

static std::string &body_buf() { static std::string s; return s; }  // last fetched page
static std::string &rb1() { static std::string s; return s; }       // regex json, nesting level 0
static std::string &rb2() { static std::string s; return s; }       // regex json, nested level
static std::string &reply() { static std::string s; return s; }
static std::string &inbuf() { static std::string s; return s; }
static unsigned g_last_len = 0;

static const char BASE[] = "https://fanfox.net";

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

// ── search ───────────────────────────────────────────────────────────────
static void search_reply(const std::string &q) {
    char enc[512] = "";
    int n = url_encode((unsigned)(size_t)q.data(), (unsigned)q.size(), (unsigned)(size_t)enc, sizeof enc - 1);
    if (n > 0)
        enc[n] = 0;
    if (!fetch(std::string(BASE) + "/search?title=" + enc, "{}"))
        return reply_err("fetch failed");
    auto anchors = rx("<a href=\"/manga/([a-z0-9_]+)/\" title=\"([^\"]+)\">", body_buf(), rb1());
    auto covers = rx("<img class=\"manga-list-4-cover\" src=\"([^\"]+)\"", body_buf(), rb2());
    std::set<std::string> seen;
    std::vector<std::pair<std::string, std::string>> items;
    for (auto &r : anchors) {
        if (r.size() < 3) continue;
        if (seen.insert(r[1]).second) items.emplace_back(r[1], r[2]);
    }
    emit("{\"ok\":true,\"data\":{\"items\":[");
    for (size_t i = 0; i < items.size(); ++i) {
        if (i) emit(",");
        emit("{\"id\":");
        jemit(items[i].first);
        emit(",\"title\":");
        jemit(items[i].second);
        emit(",\"cover\":");
        if (i < covers.size() && covers[i].size() > 1)
            jemit(covers[i][1]);
        else
            emit("\"\"");
        emit("}");
    }
    char tail[32];
    snprintf(tail, sizeof tail, "],\"total\":%zu}}", items.size());
    emit(tail);
}

// ── detail ───────────────────────────────────────────────────────────────
static void detail_reply(const std::string &slug) {
    if (!fetch(std::string(BASE) + "/manga/" + slug + "/", "{}"))
        return reply_err("fetch failed");
    auto one = [&](const char *pat) -> std::string {
        auto r = rx(pat, body_buf(), rb1());
        return r.size() && r[0].size() > 1 ? r[0][1] : std::string();
    };
    const std::string title = one("detail-info-right-title-font\">([^<]+)");
    const std::string status = one("title-tip\">([^<]+)");
    const std::string cover = one("og:image\" content=\"([^\"]+)");
    const std::string author = one("Author: <a[^>]*>([^<]+)</a>");
    const std::string artist = one("Artist: <a[^>]*>([^<]+)</a>");
    std::string desc;
    auto d = rx("(?s)detail-info-right-content\">(.*?)</p>", body_buf(), rb1());
    if (d.size() && d[0].size() > 1) {
        std::string sb(1 << 15, '\0');
        int n = text_strip_html((unsigned)(size_t)d[0][1].data(), (unsigned)d[0][1].size(),
                                (unsigned)(size_t)&sb[0], sb.size() - 1);
        if (n > 0) desc = sb.substr(0, (size_t)n);
    }
    auto g = rx("<a href=\"/directory/[^\"]*\"[^>]*>([^<]+)</a>", body_buf(), rb2());
    emit("{\"ok\":true,\"data\":{\"id\":");
    jemit(slug);
    emit(",\"title\":");
    jemit(title);
    emit(",\"cover\":");
    jemit(cover);
    emit(",\"status\":");
    jemit(status);
    emit(",\"author\":");
    jemit(author);
    emit(",\"artist\":");
    jemit(artist);
    emit(",\"genres\":[");
    for (size_t i = 0; i < g.size(); ++i) {
        if (i) emit(",");
        jemit(g[i].size() > 1 ? g[i][1] : std::string());
    }
    emit("],\"description\":");
    jemit(desc);
    emit("}}");
}

// ── chapters (the m.fanfox detail page carries the full cNNN list) ───────
static void chapters_reply(const std::string &slug) {
    if (!fetch("https://m.fanfox.net/manga/" + slug + "/", "{}"))
        return reply_err("fetch failed");
    auto ch = rx("href=\"[^\"]*/c([0-9.]+)/1\\.html\"[^>]*?title=\"([^\"]*)\"", body_buf(), rb1());
    std::vector<std::pair<std::string, std::string>> list;  // c-token, title
    std::set<std::string> seen;
    for (auto &r : ch) {
        if (r.size() < 3) continue;
        if (seen.insert(r[1]).second) list.emplace_back(r[1], r[2]);
    }
    emit("{\"ok\":true,\"data\":[");
    for (size_t i = list.size(); i-- > 0;) {  // newest first
        if (i != list.size() - 1) emit(",");
        char nb[32];
        snprintf(nb, sizeof nb, "%g", (float)atof(list[i].first.c_str()));
        emit("{\"id\":");
        jemit(slug + ":" + list[i].first);
        emit(",\"number\":");
        emit(nb);
        emit(",\"title\":");
        jemit(list[i].second);
        emit("}");
    }
    emit("]}");
}

// ── Dean-Edwards unpacker for the chapterfun payload ─────────────────────
static std::string unpack(const std::string &packed) {
    std::string local;
    auto rows = rx("\\}\\('(.*)',([0-9]+),([0-9]+),'([^']*)'\\\\.split", packed, local);
    if (rows.empty() || rows[0].size() < 5) return "";
    const std::string payload = rows[0][1];
    const int base = atoi(rows[0][2].c_str());
    const int count = atoi(rows[0][3].c_str());
    std::vector<std::string> words;
    {
        const std::string w = rows[0][4];
        size_t a = 0;
        for (size_t i = 0;; ++i) {
            if (i == w.size() || w[i] == '|') {
                words.push_back(w.substr(a, i - a));
                a = i + 1;
                if (i == w.size()) break;
            }
        }
    }
    auto unesc = [](const std::string &s) {
        std::string o;
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '\\' && i + 1 < s.size()) {
                char e = s[++i];
                o += e == 'n' ? '\n' : e;
            } else {
                o += s[i];
            }
        }
        return o;
    };
    std::string text = unesc(payload);
    for (auto &w : words) w = unesc(w);
    auto enc = [&](int c) {
        std::string r;
        do {
            int m = c % base;
            c /= base;
            char digit = m < 10 ? (char)('0' + m) : m < 36 ? (char)('a' + m - 10) : (char)(m + 29);
            r = std::string(1, digit) + r;
        } while (c > 0);
        return r;
    };
    auto tok_repl = [&](const std::string &from, const std::string &to) {
        if (from.empty()) return;
        size_t pos = 0;
        while ((pos = text.find(from, pos)) != std::string::npos) {
            bool lb = pos == 0 || !isalnum((unsigned char)text[pos - 1]);
            size_t end = pos + from.size();
            bool rbnd = end >= text.size() || !isalnum((unsigned char)text[end]);
            if (lb && rbnd && !to.empty()) {
                text.replace(pos, from.size(), to);
                pos += to.size();
            } else {
                pos = end;
            }
        }
    };
    for (int c = count - 1; c >= 1; --c) {
        std::string w = c < (int)words.size() ? words[(size_t)c] : std::string();
        tok_repl(enc(c), w.empty() ? enc(c) : w);  // canonical: empty word keeps the token
    }
    return text;
}

// ── pages ────────────────────────────────────────────────────────────────
static void pages_reply(const std::string &id) {
    size_t colon = id.find(':');
    if (colon == std::string::npos) return reply_err("bad id");
    const std::string slug = id.substr(0, colon), ch = id.substr(colon + 1);
    const size_t dot = ch.find('.');
    const std::string suffix = dot == std::string::npos ? "0" : ch.substr(dot + 1);
    const std::string reader = std::string(BASE) + "/manga/" + slug + "/" + ch + "/1.html";
    if (!fetch(reader, "{}")) return reply_err("fetch failed");
    auto t = rx("imagecount=([0-9]+)", body_buf(), rb1());
    auto cid = rx("chapterid *= *([0-9]+)", body_buf(), rb2());
    if (t.empty() || t[0].size() < 2 || cid.empty() || cid[0].size() < 2) return reply_err("no reader vars");
    const long total = atol(t[0][1].c_str());
    const std::string c = cid[0][1];
    std::set<std::string> seen;
    std::vector<std::string> urls;
    for (long k = 1; (long)urls.size() < total && k <= total + 2; k += 2) {
        const std::string cf =
            std::string(BASE) + "/chapterfun.ashx?cid=" + c + "&page=" + std::to_string(k) + "&key=";
        const std::string opts = std::string("{\"Referer\":\"") + reader + "\"}";
        if (!fetch(cf, opts.c_str())) break;
        const std::string js = unpack(body_buf());
        if (js.empty()) break;
        auto pix = rx("pix=\"([^\"]+)", js, rb1());
        auto paths = rx("\"([^\"]+\\.jpg[^\"]*)\"", js, rb2());
        if (pix.empty() || pix[0].size() < 2) break;
        const std::string base = "https:" + pix[0][1] + suffix;  // .../NNN. + suffix
        const size_t before = urls.size();
        for (auto &p : paths) {
            if (p.size() < 2 || p[1].find(".jpg") == std::string::npos) continue;
            std::string full = p[1][0] == '/' ? base + p[1] : p[1];
            if (full.compare(0, 4, "http") != 0) full = "https:" + full;
            if (seen.insert(full).second) urls.push_back(full);
        }
        if (urls.size() == before) break;  // no progress
    }
    emit("{\"ok\":true,\"data\":[");
    for (size_t i = 0; i < urls.size(); ++i) {
        if (i) emit(",");
        jemit(urls[i]);
    }
    emit("]}");
}

// ── entry ────────────────────────────────────────────────────────────────
extern "C" unsigned alloc(unsigned len) {
    inbuf().resize(len ? len : 4096);
    return (unsigned)(size_t)inbuf().data();
}

extern "C" unsigned handle(unsigned, unsigned) {
    reply().clear();
    const std::string method = env_field("method");
    if (method == "search") {
        std::string q = env_field("title");
        if (q.empty()) q = env_field("query");
        search_reply(q);
    } else if (method == "detail") {
        detail_reply(env_field("id"));
    } else if (method == "chapters") {
        chapters_reply(env_field("id"));
    } else if (method == "pages") {
        pages_reply(env_field("id"));
    } else {
        reply_err("unknown method");
    }
    g_last_len = (unsigned)reply().size();
    return (unsigned)(size_t)reply().data();
}

extern "C" unsigned last_len() { return g_last_len; }
