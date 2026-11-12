/** \file http_api.cpp
 *  \brief JSON API registration entry: builds the shared api::Ctx and hands it
 *  to the route-group TUs (http_api_{admin,library,title,reader,state,content}.cpp
 *  -- monolith split for compile parallelism, <=~200 loc per unit).
 */

#include "api_ctx.hpp"

namespace http {

void register_api(crow::SimpleApp& app, Store& store, BS::thread_pool<>& db, BS::thread_pool<>& plugin_pool,
                  plugin::Host& plugins, cache::Cache& images, reader::Worker& prefetch, reader::CoverWorker& covers,
                  const std::string& ini_path, const config::Overrides& overrides_ref) {
    api::Ctx c{.app = app,
               .store = store,
               .db = db,
               .plugin_pool = plugin_pool,
               .plugins = plugins,
               .images = images,
               .prefetch = prefetch,
               .covers = covers,
               .ini_path = ini_path,
               .overrides = overrides_ref};
    api::register_admin(c);
    api::register_library(c);
    api::register_title(c);
    api::register_reader(c);
    api::register_state(c);
    api::register_content(c);
    api::register_enrich(c);
}

} // namespace http
