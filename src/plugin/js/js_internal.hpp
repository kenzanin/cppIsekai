#pragma once

/** \file js_internal.hpp
 *  \brief Helpers shared by the js load/invoke half (js_rt.cpp) and the
 *  C bindings half (js_bindings.cpp).
 */

#include "js_rt.hpp"

#include <string>

namespace plugin::js {

/** \brief Copy a JS string value (borrowed cell); "" when it does not
 *  convert. */
std::string arg_str(JSContext* ctx, JSValueConst v);

/** \brief JSON.stringify via the realm's global object (no C-API shape
 *  bets); "" on failure. */
std::string json_stringify(JSContext* ctx, JSValueConst v);

/** \brief Install the host-tool globals (now/log/http/fs/storage) on the
 *  realm's global object. */
void bind_host_globals(JSContext* c);

} // namespace plugin::js
