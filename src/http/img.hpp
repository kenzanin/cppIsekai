#pragma once
// Pure core of GET /cache/img -- no crow, so tests run without a server:
// cache hit serves disk, miss fetches+stores then serves, failed fetch 404s.
#include "cache/cache.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

// Content-Type by file extension (cache stores the url's lowercased ext);
// unknown or absent -> octet-stream.
inline const char* image_mime(std::string_view name) {
    const std::string_view ext = name.substr(name.find_last_of('.') + 1);
    if (ext == "jpg" || ext == "jpeg") {
        return "image/jpeg";
    }
    if (ext == "png") {
        return "image/png";
    }
    if (ext == "gif") {
        return "image/gif";
    }
    if (ext == "webp") {
        return "image/webp";
    }
    if (ext == "avif") {
        return "image/avif";
    }
    if (ext == "svg") {
        return "image/svg+xml";
    }
    return "application/octet-stream";
}

// Status / body / mime of the /cache/img answer.
struct ImgReply {
    int code{};
    std::string body;
    std::string_view mime = "application/octet-stream";
};

inline ImgReply serve_cached_image(cache::Cache& images, std::string_view url) {
    const auto path = images.ensure(std::string(url), "img");
    if (!path) {
        return {404, {}, "application/octet-stream"};
    }
    std::ifstream f(*path, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return {200, ss.str(), image_mime(*path)};
}
