#pragma once

/** \file http_pages_context.hpp
 *  \brief Mustache context builders shared by the page routes. */

#include "http_routes.hpp"

#include <crow.h>

#include <string>

namespace http {

std::string title_key(const std::string& value);
crow::json::wvalue layout(Store& store, BS::thread_pool<>& db, const char* active);
crow::json::wvalue home_context(Store& store, BS::thread_pool<>& db, const crow::request& req,
                                reader::CoverWorker& covers);
crow::response html(const std::string& file, const crow::json::wvalue& ctx);

} // namespace http
