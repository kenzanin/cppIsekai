#include "fetch/fetch_internal.hpp"

#include <dlfcn.h>
#include <mutex>
#include <spdlog/spdlog.h>

// curl-impersonate ships the symbol but no header on this host (system curl
// headers predate it; see knowledge/references/dependency-api.md).
extern "C" CURLcode curl_easy_impersonate(CURL* easy, const char* client, unsigned clientscope);

namespace fetch {

const CurlApi& chrome_api() {
    static const CurlApi api{&curl_easy_init,           &curl_easy_cleanup, &curl_easy_setopt,
                             &curl_easy_perform,        &curl_easy_getinfo, &curl_slist_append,
                             &curl_easy_strerror,       &curl_easy_impersonate};
    return api;
}

/** \brief Firefox profiles are unsupported: loading libcurl-impersonate-ff
 *  (NSS) alongside the linked chrome build (BoringSSL) in one process faults
 *  in curl_multi_perform. Chrome-family profiles stay in-process. */
const CurlApi* firefox_api() {
    return nullptr;
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


} // namespace fetch
