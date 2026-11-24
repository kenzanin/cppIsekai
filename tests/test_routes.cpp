#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

// A source-split can delete a CROW_ROUTE registration without breaking the
// build or any unit test (522d800 dropped /chapter/flags this way). Every
// curl-facing route the UI depends on must still have a literal registration
// under src/http -- gate it here so the next split cannot silently remove one.
TEST_CASE("routes: curl-facing API registrations survive source splits") {
    std::string src;
    const fs::path root = fs::path(CPPISEKAI_HTTP_DIR);
    REQUIRE(fs::exists(root));
    for (const auto& entry : fs::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".cpp") continue;
        std::ifstream f(entry.path(), std::ios::binary);
        REQUIRE(f.good());
        std::ostringstream ss;
        ss << f.rdbuf();
        src += ss.str();
        src += '\n';
    }

    const char* required[] = {
        "/chapter/flags",              // generic bitfield batch (detail mark-prev / clear-next)
        "/chapter/state",              // state list for the chapter rows
        "/chapter/<string>/read",      // reader auto read bit
        "/chapter/<string>/skip",      // legacy skip bit
        "/api/library/pin",            // pin toggle on detail
        "/api/enrichment/reset",       // fetch-detail / fetch-covers reset
        "/api/migrate",                // source migration
        "/api/backup",                 // on-demand DB snapshot
        "/config",                     // settings save
    };
    for (const char* route : required) {
        const std::string needle = std::string("CROW_ROUTE(app, \"") + route + "\")";
        CHECK_MESSAGE(src.find(needle) != std::string::npos, "missing route registration: ", route);
    }
}
