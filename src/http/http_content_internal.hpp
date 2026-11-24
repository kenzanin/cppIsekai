#pragma once

/** \file http_content_internal.hpp
 *  \brief Chapter-id resolution and image-url helpers shared by the content TUs. */

#include <string>
#include <vector>

namespace http::api {

std::string envelope_error(const std::string& envelope);
std::string resolve_chapter_id(const std::string& chapters_envelope, const std::string& wanted, double chapter_number);
bool chapter_cached(const std::string& namespace_name, const std::string& slug, int chapter, int pages);
std::string pct_decode(const std::string& value);
std::vector<std::string> sibling_ids(const std::string& chapters_envelope, double chapter_number,
                                     const std::string& primary_id);

} // namespace http::api
