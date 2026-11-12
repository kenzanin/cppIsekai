#pragma once

/** \file transcode.hpp
 *  \brief Header-only image transcode (cache-layout-v3): vips buffer in ->
 *  webp|avif|jxl buffer out, optional cover resize (fit longest side, keep
 *  aspect). Decode failure returns passthrough bytes + zero dims so a weird
 *  input never fails the fetch. */

#include <vips/vips.h>

#include <string>
#include <string_view>

namespace cache {

struct TranscodeResult {
    std::string bytes;
    std::string ext; // target ext, or "" when passthrough
    int width{};
    int height{};
};

inline bool vips_ready() {
    static const bool ok = (VIPS_INIT("cppisekai") == 0);
    return ok;
}

/** \brief Transcode \p in to \p format (webp|avif|jxl|none). \p cover_max_px
 *  > 0 resizes so the longest side fits it (covers); 0 keeps dimensions. */
inline TranscodeResult transcode(std::string_view in, std::string_view format, int cover_max_px) {
    TranscodeResult passthrough{std::string(in), {}, 0, 0};
    if (format == "none" || !vips_ready())
        return passthrough;

    VipsImage* img = vips_image_new_from_buffer(in.data(), in.size(), "", nullptr);
    if (img == nullptr)
        return passthrough;

    const int w = vips_image_get_width(img);
    const int h = vips_image_get_height(img);

    VipsImage* work = img;
    if (cover_max_px > 0 && (w > cover_max_px || h > cover_max_px)) {
        VipsImage* thumb = nullptr;
        if (vips_thumbnail_image(img, &thumb, cover_max_px, "size", VIPS_SIZE_DOWN, nullptr) == 0)
            work = thumb;
    }

    void* buf = nullptr;
    size_t len = 0;
    int rc = -1;
    std::string ext;
    if (format == "webp") {
        rc = vips_webpsave_buffer(work, &buf, &len, nullptr);
        ext = "webp";
    } else if (format == "avif") {
        rc = vips_heifsave_buffer(work, &buf, &len, "compression", VIPS_FOREIGN_HEIF_COMPRESSION_AV1, nullptr);
        ext = "avif";
    } else if (format == "jxl") {
        rc = vips_jxlsave_buffer(work, &buf, &len, nullptr);
        ext = "jxl";
    }

    TranscodeResult out = passthrough;
    if (rc == 0 && buf != nullptr) {
        out.bytes.assign(static_cast<char*>(buf), len);
        out.ext = std::move(ext);
        out.width = vips_image_get_width(work);
        out.height = vips_image_get_height(work);
        g_free(buf);
    }
    if (work != img)
        g_object_unref(work);
    g_object_unref(img);
    return out;
}

} // namespace cache
