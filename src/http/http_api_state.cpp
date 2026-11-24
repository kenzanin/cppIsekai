#include "api_ctx.hpp"

#include "cache/slug.hpp"
#include "plugin/url_codec.hpp"

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace http::api {

void register_state_display(Ctx& c);
void register_state_flags(Ctx& c);
void register_state_state(Ctx& c);

void register_state(Ctx& c) {
    register_state_display(c);
    register_state_flags(c);
    register_state_state(c);
}

} // namespace http::api
