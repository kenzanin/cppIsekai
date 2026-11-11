#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <crow/json.h>
#include <crow/mustache.h>

namespace {

std::string slurp(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

void spit(const std::string& path, const std::string& data) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    f << data;
}

// 1-based line of the first differing byte (keeps a 20KB golden failure readable).
int first_diff_line(const std::string& got, const std::string& want) {
    std::size_t at = 0;
    while (at < got.size() && at < want.size() && got[at] == want[at]) ++at;
    int line = 1;
    for (std::size_t i = 0; i < at && i < got.size(); ++i)
        if (got[i] == '\n') ++line;
    return line;
}

// Render one top-level page with a representative context and diff it against
// tests/fixtures/golden/<name>.html -- locks the partial include graph and the
// context keys each page reads. CPPISEKAI_UPDATE_GOLDEN=1 rewrites the goldens
// after an intentional markup change.
void check_golden(const std::string& name, const crow::json::wvalue& ctx) {
    crow::mustache::set_base(CPPISEKAI_TEMPLATES_DIR);
    const std::string got = crow::mustache::load(name).render(ctx).dump();
    const std::string golden = std::string(CPPISEKAI_GOLDEN_DIR) + "/" + name + ".html";

    if (std::getenv("CPPISEKAI_UPDATE_GOLDEN") != nullptr) {
        spit(golden, got);
        return;
    }
    const std::string want = slurp(golden);
    if (got != want)
        MESSAGE(name, ": first mismatch at line ", first_diff_line(got, want), " (got ",
                got.size(), " bytes, golden ", want.size(), ")");
    CHECK(got == want);
}

crow::json::wvalue active(const char* section) {
    crow::json::wvalue c;
    c["active_" + std::string(section)] = true;
    return c;
}

// home_context() stand-in: populated plugin rail + saved grid so grid_body's
// list, cover and no-cover branches all render (grid.mustache reuses it).
crow::json::wvalue home_ctx() {
    auto c = active("library");
    c["q"] = "";
    c["plugin"] = "";
    c["filtered"] = false;
    c["total"] = "2";
    c["saved_mode"] = true;
    c["empty_saved"] = false;

    std::vector<crow::json::wvalue> plugins;
    {
        crow::json::wvalue p;
        p["name"] = "mangadex";
        p["logo"] = "mangadex";
        p["titles"] = "2";
        p["current"] = true;
        plugins.push_back(std::move(p));
        crow::json::wvalue alt;
        alt["name"] = "baka";
        alt["titles"] = "0"; // no logo -> {{#logo}} false branch
        plugins.push_back(std::move(alt));
    }
    c["plugins"] = std::move(plugins);

    std::vector<crow::json::wvalue> saved;
    {
        crow::json::wvalue row;
        row["name"] = "One Piece";
        row["slug"] = "one-piece";
        row["plugin"] = "mangadex";
        row["cover"] = "http://example/cover.jpg";
        row["url"] = "/view/manga/mangadex/one-piece";
        saved.push_back(std::move(row));
        crow::json::wvalue bare;
        bare["name"] = "No Cover";
        bare["slug"] = "no-cover";
        bare["plugin"] = "baka";
        bare["cover"] = ""; // {{#cover}} false -> placeholder div
        bare["url"] = "/view/manga/baka/no-cover";
        saved.push_back(std::move(bare));
    }
    c["saved_list"] = std::move(saved);
    return c;
}

crow::json::wvalue reader_ctx() {
    auto c = active("library");
    c["resume_ch"] = 12;
    c["resume_page"] = 3;
    c["resume_mode"] = "paged";
    c["resume_dir"] = "rtl";
    c["resume_zoom"] = "fit-width";
    return c;
}

crow::json::wvalue logs_ctx() {
    auto c = active("logs");
    c["lines"] = std::vector<std::string>{"first log line", "second log line"};
    return c;
}

} // namespace

TEST_CASE("templates: each top-level page renders to its golden file") {
    check_golden("home.mustache", home_ctx());
    check_golden("grid.mustache", home_ctx());
    check_golden("search.mustache", active("search"));
    check_golden("reader.mustache", reader_ctx());
    check_golden("detail.mustache", active("library"));
    check_golden("blank.mustache", active("about"));
    check_golden("plugins.mustache", active("plugins"));
    check_golden("settings.mustache", active("settings"));
    check_golden("logs.mustache", logs_ctx());
}
