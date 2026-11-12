#pragma once

/** \file img.hpp
 *  \brief Pure core of GET /cache/img -- no crow, so tests run without a
 *  server: cache hit serves disk, miss fetches+stores then serves, failed
 *  fetch 404s. */

#include "cache/cache.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

/** \brief Content-Type by file extension (cache stores the url's lowercased
 *  ext); unknown or absent -> octet-stream. */
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
    if (ext == "jxl") {
        return "image/jxl";
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
    std::optional<std::string> ratio_header; // X-Image-Ratio, covers only
};

/** \brief The /cache/img answer for \p url: 200+body+mime on hit, the 404
 *  shape otherwise. Context (cover/page) routes through Cache::img_ensure —
 *  v3 layout + transcode + sidecar (D1-D3); empty context keeps the v2
 *  sha fallback. cover=true adds the X-Image-Ratio header from the
 *  stored sidecar (D3). */
inline ImgReply serve_cached_image(cache::Cache& images, std::string_view url,
                                   const cache::Cache::CoverCtx& cover_ctx = {},
                                   const cache::Cache::PageCtx& page_ctx = {}) {
    const auto path = images.img_ensure(std::string(url), cover_ctx, page_ctx);
    if (!path) {
        return {404, {}, "application/octet-stream", std::nullopt};
    }
    std::ifstream f(*path, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    auto reply = ImgReply{200, ss.str(), image_mime(*path), std::nullopt};
    if (cover_ctx.cover) {
        const double ratio = cache::Cache::ratio_of_name(std::filesystem::path(*path).filename().string());
        if (ratio > 0.0)
            reply.ratio_header = std::to_string(ratio);
    }
    return reply;
}
