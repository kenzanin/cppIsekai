#pragma once

/** \file chrome_internal.hpp
 *  \brief Seams between the Chrome lifecycle TU and the tool-dispatch TU. */

#include "cdp/CDP.hpp"

#include <glaze/glaze.hpp>

#include <string>

namespace browser {

/** Working page owned by launch(); nullptr before boot / after teardown. */
cdp::quick::QuickPage* page();
bool alive();
bool boot_locked(std::string* err);
bool relaunch(std::string* err);
std::string arg_str(const glz::generic& args, const char* key);
int nav_timeout_ms();

} // namespace browser
