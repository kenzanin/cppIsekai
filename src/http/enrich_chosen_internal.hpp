#pragma once

/** \file enrich_chosen_internal.hpp
 *  \brief Stored-source/chosen-field helpers shared by the enrich TUs. */

#include "enrich_internal.hpp"

namespace http::api {

std::optional<crow::json::wvalue> stored_source(const std::string& fetched_json, const std::string& id,
                                                const std::string& kind, std::int64_t& out_fetched_at);
std::optional<std::string> normalize_chosen(const crow::json::rvalue& ch);

/** Read a field from an already-parsed `chosen` object (no extra DB call). */
std::optional<std::string> chosen_field(const glz::generic& chosen, const char* key);

std::optional<std::string> chosen_title(Store& store, BS::thread_pool<>& db, const std::string& plugin,
                                        const std::string& slug);
std::optional<std::string> chosen_cover(Store& store, BS::thread_pool<>& db, const std::string& plugin,
                                        const std::string& slug);

} // namespace http::api
