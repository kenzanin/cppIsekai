#include "api_ctx.hpp"

#include "img.hpp"

#include <SimpleIni.h>

#include <fstream>
#include <filesystem>
#include <map>
#include <sstream>
#include <system_error>
#include <vector>

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace http::api {

void register_admin_plugins(Ctx& c);
void register_admin_config(Ctx& c);
void register_admin_system(Ctx& c);

void register_admin(Ctx& c) {
    register_admin_plugins(c);
    register_admin_config(c);
    register_admin_system(c);
}

} // namespace http::api
