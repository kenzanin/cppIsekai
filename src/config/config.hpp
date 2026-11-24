#pragma once

/** \file config.hpp
 *  \brief Config core: layered resolve (default < ini < env < explicit flag)
 *  over the registry. All config knowledge lives in src/config/ (D1);
 *  header-only, no .cpp. */

#include "registry.hpp"

#include <CLI/CLI.hpp>
#include "glaze/glaze.hpp"

#include <SimpleIni.h> // vendored 4.27; load is read-only, save() below is the comment-safe merge

#include <array>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>


#include "config/config_types.hpp"
#include "config/config_resolve.hpp"
#include "config/config_io.hpp"
