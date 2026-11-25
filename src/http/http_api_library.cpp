#include "api_ctx.hpp"
#include "enrich/enrich_internal.hpp"

namespace http::api {

void register_library_read(Ctx& c);
void register_library_write(Ctx& c);

void register_library(Ctx& c) {
    register_library_read(c);
    register_library_write(c);
}

} // namespace http::api
