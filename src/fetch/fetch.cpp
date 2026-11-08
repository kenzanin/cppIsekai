#include "fetch/fetch.hpp"

#include <algorithm>
#include <cctype>
#include <dlfcn.h>
#include <mutex>

#include <spdlog/spdlog.h>

#ifndef CPPISEKAI_FETCH_LIB_DIR
#define CPPISEKAI_FETCH_LIB_DIR "/usr/lib"
#endif

// curl-impersonate ships the symbol but no header on this host (system curl
// headers predate it; see knowledge/references/dependency-api.md). The chrome
// family is linked directly; the firefox family binds the same symbols from
// its dlopen'd sibling below.
extern "C" CURLcode curl_easy_impersonate(CURL* easy, const char* client, unsigned clientscope);

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

/** \brief The one fn-table per family: chrome binds the linked lib, firefox
 *  binds its dlopen'd sibling. A handle is always used with its own table. */
struct CurlApi {
    CURL* (*easy_init)();
    void (*easy_cleanup)(CURL*);
    CURLcode (*easy_setopt)(CURL*, CURLoption, ...);
    CURLcode (*easy_perform)(CURL*);
    CURLcode (*easy_getinfo)(CURL*, CURLINFO, ...);
    curl_slist* (*slist_append)(curl_slist*, const char*);
    const char* (*easy_strerror)(CURLcode);
    CURLcode (*easy_impersonate)(CURL*, const char*, unsigned);
};

const CurlApi& chrome_api() {
    static const CurlApi api{&curl_easy_init,           &curl_easy_cleanup, &curl_easy_setopt,
                             &curl_easy_perform,        &curl_easy_getinfo, &curl_slist_append,
                             &curl_easy_strerror,       &curl_easy_impersonate};
    return api;
}

/** \brief Lazy dlopen of the firefox family (NSS build, v0.61 pair of the
 *  vendored chrome lib). nullptr after a failed load; the request then runs
 *  on the chrome engine with a warn-once. */
const CurlApi* firefox_api() {
    static std::once_flag once;
    static CurlApi api{};
    static bool ok = false;
    std::call_once(once, [] {
        const std::string base = CPPISEKAI_FETCH_LIB_DIR;
        void* h = dlopen((base + "/libcurl-impersonate-ff.so.4").c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!h) {
            h = dlopen("libcurl-impersonate-ff.so.4", RTLD_NOW | RTLD_LOCAL);
        }
        if (!h) {
            spdlog::warn("[fetch] firefox lib dlopen failed ({}); ff* profiles run on the chrome engine", dlerror());
            return;
        }
        const auto sym = [&h](const char* n) { return dlsym(h, n); };
        api.easy_init = reinterpret_cast<CURL* (*)()>(sym("curl_easy_init"));
        api.easy_cleanup = reinterpret_cast<void (*)(CURL*)>(sym("curl_easy_cleanup"));
        api.easy_setopt = reinterpret_cast<CURLcode (*)(CURL*, CURLoption, ...)>(sym("curl_easy_setopt"));
        api.easy_perform = reinterpret_cast<CURLcode (*)(CURL*)>(sym("curl_easy_perform"));
        api.easy_getinfo = reinterpret_cast<CURLcode (*)(CURL*, CURLINFO, ...)>(sym("curl_easy_getinfo"));
        api.slist_append = reinterpret_cast<curl_slist* (*)(curl_slist*, const char*)>(sym("curl_slist_append"));
        api.easy_strerror = reinterpret_cast<const char* (*)(CURLcode)>(sym("curl_easy_strerror"));
        api.easy_impersonate =
            reinterpret_cast<CURLcode (*)(CURL*, const char*, unsigned)>(sym("curl_easy_impersonate"));
        if (!api.easy_init || !api.easy_cleanup || !api.easy_setopt || !api.easy_perform || !api.easy_getinfo ||
            !api.slist_append || !api.easy_strerror || !api.easy_impersonate) {
            spdlog::warn("[fetch] firefox lib missing symbols; ff* profiles run on the chrome engine");
            return;
        }
        ok = true;
    });
    return ok ? &api : nullptr;
}

/** \brief The system libcurl family: dlopen'd the same way as the ff lib, but
 *  OpenSSL TLS and NO impersonation -- the only stack proven to handshake with
 *  uploads.mangadex.org's CDN (the impersonate builds' TLS dies there, and
 *  plain curl always succeeded). Absent lib => nullptr, the caller degrades. */
const CurlApi* system_api() {
    static std::once_flag once;
    static CurlApi api{};
    static bool ok = false;
    std::call_once(once, [] {
        void* h = dlopen("libcurl.so.4", RTLD_NOW | RTLD_LOCAL);
        if (!h) {
            spdlog::warn("[fetch] system libcurl dlopen failed ({}); system family unavailable", dlerror());
            return;
        }
        const auto sym = [&h](const char* n) { return dlsym(h, n); };
        // This lib's global init never ran (the linked impersonate lib did its
        // own): two libcurl instances in one process need independent SSL init,
        // else the first easy_perform segfaults.
        const auto global_init = reinterpret_cast<CURLcode (*)(long)>(sym("curl_global_init"));
        if (!global_init || global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
            spdlog::warn("[fetch] system libcurl global_init failed; system family unavailable");
            return;
        }
        api.easy_init = reinterpret_cast<CURL* (*)()>(sym("curl_easy_init"));
        api.easy_cleanup = reinterpret_cast<void (*)(CURL*)>(sym("curl_easy_cleanup"));
        api.easy_setopt = reinterpret_cast<CURLcode (*)(CURL*, CURLoption, ...)>(sym("curl_easy_setopt"));
        api.easy_perform = reinterpret_cast<CURLcode (*)(CURL*)>(sym("curl_easy_perform"));
        api.easy_getinfo = reinterpret_cast<CURLcode (*)(CURL*, CURLINFO, ...)>(sym("curl_easy_getinfo"));
        api.slist_append = reinterpret_cast<curl_slist* (*)(curl_slist*, const char*)>(sym("curl_slist_append"));
        api.easy_strerror = reinterpret_cast<const char* (*)(CURLcode)>(sym("curl_easy_strerror"));
        // No easy_impersonate on purpose: the system family never impersonates.
        if (!api.easy_init || !api.easy_cleanup || !api.easy_setopt || !api.easy_perform || !api.easy_getinfo ||
            !api.slist_append || !api.easy_strerror) {
            spdlog::warn("[fetch] system libcurl missing symbols; system family unavailable");
            return;
        }
        ok = true;
    });
    return ok ? &api : nullptr;
}

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
            hdrs = c.slist_append(hdrs, (k + ": " + v).c_str());
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

Result sync(const Request& req, const std::string& proxy) {
    std::string profile = req.profile.empty() ? std::string("chrome116") : req.profile;
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
        if (const CurlApi* ff = firefox_api()) {
            return run_with(*ff, req, profile, proxy);
        }
        // The lib is absent on this host: run on the chrome engine (the
        // impersonate call no-ops there) rather than fail the request.
    }
    return run_with(chrome_api(), req, profile, proxy);
}

} // namespace fetch
