#include "api_ctx.hpp"

#include "cache/slug.hpp"
#include "export_cbz.hpp"
#include "img.hpp"
#include "pages_api.hpp"

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace http::api {

// Percent-decode a query value (crow's param() does this internally; the
// raw_url path below must match or curl receives a literal "%3A%2F" host).
std::string pct_decode(const std::string& s) {
    auto nib = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size() && nib(s[i + 1]) >= 0 && nib(s[i + 2]) >= 0) {
            out += static_cast<char>(nib(s[i + 1]) * 16 + nib(s[i + 2]));
            i += 2;
        } else {
            out += s[i];
        }
    }
    return out;
}

} // namespace http::api
