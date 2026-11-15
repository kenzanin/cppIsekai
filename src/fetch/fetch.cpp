#include "fetch/fetch.hpp"
#include "fetch/fetch_internal.hpp"

#include <algorithm>
#include <cctype>

#include <cstdio>

#include <spdlog/spdlog.h>

namespace fetch {

namespace {

struct Sink {
    std::string body;
    std::map<std::string, std::string> headers; // keys lowercased, last wins
};

size_t on_body(char* ptr, size_t size, size_t nmemb, void* ud) {
    static_cast<std::string*>(ud)->append(ptr, size * nmemb);
    return size * nmemb;
}

size_t on_header(char* ptr, size_t size, size_t nmemb, void* ud) {
    auto* headers = static_cast<std::map<std::string, std::string>*>(ud);
    std::string_view line(ptr, size * nmemb);
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
        line.remove_suffix(1);
    }
    const auto colon = line.find(':');
    if (colon == std::string_view::npos) {
        return size * nmemb; // status line or obs-fold continuation
    }
    std::string key(line.substr(0, colon));
    std::string value(line.substr(colon + 1));
    auto trim = [](std::string& s) {
        const auto issp = [](unsigned char c) { return c == ' ' || c == '\t'; };
        while (!s.empty() && issp(s.front())) {
            s.erase(s.begin());
        }
        while (!s.empty() && issp(s.back())) {
            s.pop_back();
        }
    };
    std::ranges::transform(key, key.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    trim(key);
    trim(value);
    if (!key.empty()) {
        (*headers)[key] = std::move(value);
    }
    return size * nmemb;
}

struct CurlDeleter {
    void (*fn)(CURL*);
    void operator()(CURL* p) const { fn(p); }
};

using CurlPtr = std::unique_ptr<CURL, CurlDeleter>;

/** \brief The single transfer body, family-parameterized. The option block,
 *  the retry and the proxy rule are identical for both libs. */
Result run_with(const CurlApi& c, const Request& req, const std::string& profile, const std::string& proxy) {
    Result out;
    CurlPtr easy(c.easy_init(), CurlDeleter{c.easy_cleanup});
    if (!easy) {
        out.error = "curl init failed";
        return out;
    }

    Sink sink;
    // One option block for both attempts: the retry path re-creates the
    // handle, and headers/method/body must survive it (they did not before).
    const auto setup = [&](const CurlPtr& e) {
        curl_slist* hdrs = nullptr;
        if (!req.accept_header.empty()) {
            hdrs = c.slist_append(hdrs, ("Accept: " + req.accept_header).c_str());
        }
        if (!req.ua_header.empty()) {
            hdrs = c.slist_append(hdrs, ("User-Agent: " + req.ua_header).c_str());
        }
        for (const auto& [k, v] : req.headers) {
            std::string header = k;
            header += ": ";
            header += v;
            hdrs = c.slist_append(hdrs, header.c_str());
        }
        c.easy_setopt(e.get(), CURLOPT_URL, req.url.c_str());
        c.easy_setopt(e.get(), CURLOPT_FOLLOWLOCATION, 1L);
        c.easy_setopt(e.get(), CURLOPT_WRITEFUNCTION, on_body);
        c.easy_setopt(e.get(), CURLOPT_WRITEDATA, &sink.body);
        c.easy_setopt(e.get(), CURLOPT_HEADERFUNCTION, on_header);
        c.easy_setopt(e.get(), CURLOPT_HEADERDATA, &sink.headers);
        c.easy_setopt(e.get(), CURLOPT_TIMEOUT_MS, std::max(1L, req.timeout_ms));
        c.easy_setopt(e.get(), CURLOPT_CAINFO, "/etc/ssl/certs/ca-certificates.crt");
        applyProxy(e.get(), proxy); // host-wide; empty = direct
        if (!req.cookie_jar.empty()) {
            // 2.1: load on start, save on handle cleanup. A not-yet-existing
            // file just starts an empty engine, so no pre-check is needed.
            c.easy_setopt(e.get(), CURLOPT_COOKIEFILE, req.cookie_jar.c_str());
            c.easy_setopt(e.get(), CURLOPT_COOKIEJAR, req.cookie_jar.c_str());
        }
        if (req.method != "GET") {
            c.easy_setopt(e.get(), CURLOPT_CUSTOMREQUEST, req.method.c_str());
        }
        if (!req.body.empty()) {
            c.easy_setopt(e.get(), CURLOPT_POSTFIELDS, req.body.c_str()); // lives as long as this call
            c.easy_setopt(e.get(), CURLOPT_POSTFIELDSIZE, static_cast<long>(req.body.size()));
        }
        if (hdrs) {
            c.easy_setopt(e.get(), CURLOPT_HTTPHEADER, hdrs); // freed with the handle's cache; lives this call
        }
        if (!req.plain) {
            c.easy_impersonate(e.get(), profile.c_str(), 1);
        }
    };
    setup(easy);
    // ponytail: the impersonate builds intermittently fail the TLS handshake
    // (ECONNRESET) against big LBs; one fresh-handle retry removes it.
    CURLcode rc = CURLE_OK;
    for (int attempt = 0; attempt < 2; ++attempt) {
        rc = c.easy_perform(easy.get());
        if (rc == CURLE_OK || rc != CURLE_SSL_CONNECT_ERROR) {
            break;
        }
        easy.reset(c.easy_init());
        if (!easy) {
            out.error = "curl re-init failed";
            return out;
        }
        setup(easy);
    }
    if (rc != CURLE_OK) {
        out.error = c.easy_strerror(rc);
        return out;
    }

    c.easy_getinfo(easy.get(), CURLINFO_RESPONSE_CODE, &out.status);
    out.body = std::move(sink.body);
    out.headers = std::move(sink.headers);
    return out;
}

} // namespace

namespace {

/** \brief Shell-single-quote for popen command lines. */
std::string sh_quote(const std::string& s) {
    std::string out = "'";
    for (const char c : s) {
        if (c == '\'') {
            out += "'\\''";
        } else {
            out += c;
        }
    }
    out += '\'';
    return out;
}

/** \brief Headless-renderer fetch for CF-challenge sites (4ucomics): the real
 *  browser engine solves the challenge; --dump html feeds the normal lexbor
 *  pipeline unchanged. Stateless per request -- verified to solve without
 *  persisted cookies. GET only: POST/header/cookie scrapes keep curl engines
 *  (profile rewritten to chrome116 by the caller below). */
Result rendered_fetch(const Request& req) {
    const bool obscura = req.profile == "obscura";
    const std::string tool = obscura ? "obscura" : "lightpanda";
    Result r;
    // coreutils guard on top of the tool's own waits (nightlies hang).
    const std::string secs = std::to_string(req.timeout_ms / 1000 + 15);
    std::string cmd = "timeout " + secs + " ";
    if (obscura) {
        // --stealth: consistent fingerprint + TLS impersonation. storage-dir is
        // best-effort (nightly persists nothing) but harmless.
        cmd += "obscura --stealth --storage-dir data/obscura fetch " + sh_quote(req.url) +
               " --dump html --wait-until networkidle2 --timeout " + std::to_string(req.timeout_ms);
    } else {
        // 8s: enough for the challenge JS; more just burns the budget.
        cmd += "lightpanda fetch " + sh_quote(req.url) + " --dump html --wait-ms 8000";
    }
    FILE* p = popen(cmd.c_str(), "r");
    if (!p) {
        r.error = tool + ": popen failed";
        return r;
    }
    std::array<char, 65536> buf{};
    size_t n = 0;
    while ((n = fread(buf.data(), 1, buf.size(), p)) > 0) {
        r.body.append(buf.data(), n);
    }
    const int rc = pclose(p);
    if (rc != 0) {
        r.body.clear();
        r.error = tool + " exit " + std::to_string(rc);
        return r;
    }
    if (r.body.empty()) {
        r.error = tool + ": empty body";
        return r;
    }
    r.status = 200;
    return r;
}

} // namespace

Result sync(const Request& req, const std::string& proxy) {
    std::string profile = req.profile.empty() ? std::string("chrome116") : req.profile;
    if ((profile == "lightpanda" || profile == "obscura") && (req.method.empty() || req.method == "GET")) {
        return rendered_fetch(req);
    }
    if (profile != "lightpanda" && profile != "obscura") {
        // fallthrough: the curl gates below
    } else {
        profile = "chrome116"; // non-GET on a challenged site: rare, curl still tries
    }
    if (req.plain) {
        // The system family: stock TLS, no impersonation -- the only engine
        // proven to handshake with uploads.mangadex.org's CDN. Degrades to
        // the linked chrome lib (impersonation skipped) if dlopen fails.
        if (const CurlApi* sys = system_api()) {
            return run_with(*sys, req, profile, proxy);
        }
        return run_with(chrome_api(), req, profile, proxy);
    }
    if (family_of(profile) == Family::Firefox) {
        // ff/tor: served by system OpenSSL (NSS vs BoringSSL cannot share one
        // process). Honest degradation: plain TLS, no impersonation. Never
        // silently run on chrome-with-no-impersonation. If system libcurl is
        // absent, fall through to the chrome engine as last resort. */
        if (const CurlApi* sys = system_api()) {
            return run_with(*sys, req, profile, proxy);
        }
    }
    // Chrome/edge/safari: impersonate lib, with system OpenSSL fallback after
    // repeated TLS handshake failures (ponytail: impersonate builds intermittently
    // drop the handshake against big LBs).
    if (const CurlApi* sys = system_api()) {
        Result r = run_with(chrome_api(), req, profile, proxy);
        if (r.error.empty() || !r.error.contains("SSL_CONNECT_ERROR")) {
            return r;
        }
        // Impersonate TLS failed once; retry once on system OpenSSL before giving up.
        r = run_with(*sys, req, profile, proxy);
        if (r.error.empty()) {
            return r;
        }
        // Return the impersonate error (more diagnostic) if system also fails.
        Result imp = run_with(chrome_api(), req, profile, proxy);
        return imp.error.empty() ? imp : r;
    }
    return run_with(chrome_api(), req, profile, proxy);
}

} // namespace fetch
