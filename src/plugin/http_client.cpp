#include "http_client.hpp"

namespace plugin {

FetchResult fetch_sync(const std::string& url, long timeout_ms, const std::string& profile, const std::string& method,
                       const std::string& body, const std::string& accept_header, const std::string& ua_header,
                       const std::vector<std::pair<std::string, std::string>>& extra_headers,
                       const std::string& proxy) {
    fetch::Request req;
    req.url = url;
    req.timeout_ms = timeout_ms;
    req.profile = profile;
    req.method = method;
    req.body = body;
    req.accept_header = accept_header;
    req.ua_header = ua_header;
    req.headers = extra_headers;
    return fetch::sync(req, proxy);
}

} // namespace plugin
