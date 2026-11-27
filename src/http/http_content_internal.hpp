#pragma once

/** \file http_content_internal.hpp
 *  \brief Chapter-id resolution and image-url helpers shared by the content TUs. */

#include <string>
#include <vector>

namespace http::api {

bool chapter_cached(const std::string& namespace_name, const std::string& slug, int chapter, int pages);
std::string pct_decode(const std::string& value);

} // namespace http::api
