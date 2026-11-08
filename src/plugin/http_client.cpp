#include "http_client.hpp"

#include "plugin.hpp"

#include <algorithm>
#include <cctype>

namespace plugin {

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

FetchResult fetch_sync(const std::string& url, long timeout_ms, const std::string& profile, const std::string& method,
                       const std::string& body, const std::string& accept_header, const std::string& ua_header,
                       const std::vector<std::pair<std::string, std::string>>& extra_headers,
                       const std::string& proxy) {
    FetchResult out;
    CurlPtr easy(curl_easy_init(), &curl_easy_cleanup);
    if (!easy) {
        out.error = "curl init failed";
        return out;
    }

    FetchSink sink;
    // One option block for both attempts: the retry path re-creates the
    // handle, and headers/method/body must survive it (they did not before).
    const auto setup = [&](const CurlPtr& e) {
        curl_slist* hdrs = nullptr;
        if (!accept_header.empty()) {
            hdrs = curl_slist_append(hdrs, ("Accept: " + accept_header).c_str());
        }
        if (!ua_header.empty()) {
            hdrs = curl_slist_append(hdrs, ("User-Agent: " + ua_header).c_str());
        }
        for (const auto& [k, v] : extra_headers) {
            hdrs = curl_slist_append(hdrs, (k + ": " + v).c_str());
        }
        curl_easy_setopt(e.get(), CURLOPT_URL, url.c_str());
        curl_easy_setopt(e.get(), CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(e.get(), CURLOPT_WRITEFUNCTION, on_body);
        curl_easy_setopt(e.get(), CURLOPT_WRITEDATA, &sink.body);
        curl_easy_setopt(e.get(), CURLOPT_HEADERFUNCTION, on_header);
        curl_easy_setopt(e.get(), CURLOPT_HEADERDATA, &sink.headers);
        curl_easy_setopt(e.get(), CURLOPT_TIMEOUT_MS, std::max(1L, timeout_ms));
        curl_easy_setopt(e.get(), CURLOPT_CAINFO, "/etc/ssl/certs/ca-certificates.crt");
        applyProxy(e.get(), proxy); // host-wide; empty = direct
        if (method != "GET") {
            curl_easy_setopt(e.get(), CURLOPT_CUSTOMREQUEST, method.c_str());
        }
        if (!body.empty()) {
            curl_easy_setopt(e.get(), CURLOPT_POSTFIELDS, body.c_str()); // lives as long as this call
            curl_easy_setopt(e.get(), CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
        }
        if (hdrs) {
            curl_easy_setopt(e.get(), CURLOPT_HTTPHEADER, hdrs); // freed with the handle's cache; lives this call
        }
        curl_easy_impersonate(e.get(), profile.c_str(), 1);
    };
    setup(easy);
    // ponytail: this impersonate build intermittently fails TLS handshake
    // (ECONNRESET) against MangaDex's LB; one fresh-handle retry removes it.
    CURLcode rc = CURLE_OK;
    for (int attempt = 0; attempt < 2; ++attempt) {
        rc = curl_easy_perform(easy.get());
        if (rc == CURLE_OK || rc != CURLE_SSL_CONNECT_ERROR) {
            break;
        }
        easy.reset(curl_easy_init());
        if (!easy) {
            out.error = "curl re-init failed";
            return out;
        }
        setup(easy);
    }
    if (rc != CURLE_OK) {
        out.error = curl_easy_strerror(rc);
        return out;
    }

    curl_easy_getinfo(easy.get(), CURLINFO_RESPONSE_CODE, &out.status);
    out.body = std::move(sink.body);
    out.headers = std::move(sink.headers);
    return out;
}

} // namespace plugin
