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
        row["dup"] = 3; // library-dedup 3.1: data-dup attrs exercised in golden
        saved.push_back(std::move(row));
        crow::json::wvalue bare;
        bare["name"] = "No Cover";
        bare["slug"] = "no-cover";
        bare["plugin"] = "baka";
        bare["cover"] = ""; // {{#cover}} false -> placeholder div
        bare["url"] = "/view/manga/baka/no-cover";
        bare["dup"] = 0;
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

// Detail-page badge lock (status-flags-followup-cleanup): every badge is
// gated on its own status_flags bit (bit0 New, bit1 Done, bits2-5 the status
// category, bit6 Dim). The badge text is static; the gate is the bit mask.
TEST_CASE("templates: detail badges are gated on their status_flags bit") {
    crow::mustache::set_base(CPPISEKAI_TEMPLATES_DIR);
    const std::string got = crow::mustache::load("detail.mustache").render(active("library")).dump();

    const std::pair<const char*, int> badges[] = {
        {"New", 1}, {"Done", 2}, {"Ongoing", 4}, {"Hiatus", 8}, {"Canceled", 16}, {"Completed", 32}, {"Dim", 64},
    };
    for (const auto& [label, bit] : badges) {
        const std::string gate = "x-show=\"detail.status_flags & " + std::to_string(bit) + "\"";
        const auto at = got.find(">" + std::string(label) + "</span>");
        REQUIRE(at != std::string::npos);
        const auto start = got.rfind("<span", at);
        REQUIRE(start != std::string::npos);
        const std::string span = got.substr(start, at - start);
        CHECK(span.find(gate) != std::string::npos);
    }
    // the dead progressPct/isDone reference is gone
    CHECK(got.find("progressPct") == std::string::npos);
    CHECK(got.find("isDone") == std::string::npos);
}

// Badge semantics lock (library-grid-stats): New/Done come from the stored
// status_flags bitfield (bit0=is_new, bit1=is_done), not from client-side
// arithmetic over dataset.read/total/update. A golden regen could silently
// re-derive the badge in the page; this asserts the rule, not the bytes.
TEST_CASE("templates: card badge reads stored status_flags bits") {
    crow::mustache::set_base(CPPISEKAI_TEMPLATES_DIR);
    const std::string got = crow::mustache::load("home.mustache").render(home_ctx()).dump();

    // open tag of the badge span (x-show + class), sans trailing '>'.
    auto open_tag = [&got](const char* label) {
        const auto at = got.find(std::string(">").append(label).append("</span>"));
        if (at == std::string::npos) return std::string();
        const auto start = got.rfind("<span", at);
        return start == std::string::npos ? std::string() : got.substr(start, at - start);
    };

    const std::string done = open_tag("Done");
    CHECK(done.find("x-show") != std::string::npos); // span matched, not a degenerate
    CHECK(done.find(".done") != std::string::npos);    // gated by the stored bit1
    CHECK(done.find("dataset.read") == std::string::npos);  // no client-side arithmetic
    CHECK(done.find("dataset.total") == std::string::npos);
    CHECK(done.find("dataset.update") == std::string::npos);

    const std::string upd = open_tag("New");
    CHECK(upd.find("x-show") != std::string::npos);
    CHECK(upd.find(".news") != std::string::npos);   // gated by the stored bit0
    CHECK(upd.find("dataset.update") == std::string::npos);

    // the row carries the bitfield the grid reads.
    CHECK(got.find("data-status-flags=") != std::string::npos);
}
