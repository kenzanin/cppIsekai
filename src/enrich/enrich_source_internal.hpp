#pragma once

/** \file enrich_source_internal.hpp
 *  \brief Fallback title candidates for the metadata source route. */

#include "enrich_internal.hpp"

#include <string>
#include <vector>

namespace http::api {

std::vector<std::string> retry_titles(Store& store, const std::optional<Store::TitleMetaRow>& title_meta,
                                      const std::string& plugin, const std::string& slug,
                                      const std::string& first);

} // namespace http::api
