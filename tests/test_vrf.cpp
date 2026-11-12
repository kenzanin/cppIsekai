#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <map>
#include <string>
#include <vector>

#include "plugin/vrf.hpp"

namespace {

// Real mangafire VRF tables (rotates with keiyoushi updates) — same vectors
// as goIsekai's vrf_sign_test.go; byte-for-byte parity required.
const char* VRF_K1 = "0Ec58JOY3uBzJK9m3zqIOpdlF7UFiax9DmA=";
const char* VRF_T1 = "yINlmUNho8VYJT+ibTIP+9ESiULpVEtMOoD6U6lRE0R/xwXo/Xp9NrUgC4cw/Lmo33vUyjUE40kUoEWIr/fxfNNcq2s79ShQ5NhNrFnJ4hXPwOu/SuXzIbuTQKGFvfm08E9jvCfqAtoDqvQq3dVWPQFmJjgvkISBeXY3BgANR+yVnjGbcxZ47d6kLNfZPIayTq3/YGySb1KuVZodWp/WGNAO5pfMcpaK53Hhs0allBszaMaxuouOwdxbwgxIw6YunSsXjI05Yi0j9j4eHKfSXR8Ifo/Od+8iamRfCXTyvm7NGRGYdcQ0ywcK/u6RXhrbcCm4t2eCtrDgQVecJGkQ+A==";
const char* VRF_K2 = "AAdjb1iPY8CiDmq9H34tKTBF8a3oDQ==";
const char* VRF_T2 = "IUFltCxD3Oc2cwCgkJffthaOg9cgPUb0LgW6H/VtfcF0kc5F25t+aWj6JH9VOhOaY0rAFdUxlDnl5BLNvwEJvQtP5qcw7vdb/K+chnbwnspSHT8mz5lqwz41TezG0hkO06FTjJZhsyNuFLDpD2ZZxQj/QIRcF90zpmQ7Byu483WsQqUE0C342HL+JXngRB6fRzxRyVTaKu83h7UYTJ0QMt6ixFh6S3F8gqkKwrGTL3jHNBsD45UnifK8+RGtishQV2K3rujLKEkiZxpr2dYcudFW4oFsDKhad3CLBvuyTqsCo4B7mL5IKQ1vXo/MOOvq1I1d8ar9X6Ttu5KF4fZgiA==";
const char* VRF_K3 = "DELOJgPsVaCcblDtTGMdHzM=";
const char* VRF_T3 = "NQHlu1/wVO5EmkwQymF810qqY2xG1k2obcas4Z9mCsPEIFl9pRIjFxbJ7ybMHbBckT5Ton85E0FOeHezbh/mjlEYpmpnlXOS8dgrqeq2KfxImTh1YK9y0PeMNhzA1OQzSY9brYOJq/l2QnE/hwOeZIhPixVSKIUlDb5vLcH6RWKxkIEMuP0bDwIqQ71AJJaEaMJL7A6YtyIwoRT+L5v4aZzodN/0+3nOGsfblFjgxSfPzVDjNFeNl5P26+kEC/8AHgdrpAbt3hHz3HrRN1Y6e+JHgF7ncFWnoF0y3THL1S71WgWGCa6KtSzTCCG58n68nTyj2T3Sshk7utqCtMi/ZQ==";

std::vector<plugin::VrfStage> stages() {
    return {{0x5A, VRF_K1, VRF_T1}, {0x35, VRF_K2, VRF_T2}, {0xBA, VRF_K3, VRF_T3}};
}

} // namespace

TEST_CASE("vrf golden vectors match goIsekai byte-for-byte") {
    std::string err;
    struct Vec {
        const char* name;
        const char* path;
        std::map<std::string, std::string> params;
        const char* want;
    };
    const Vec vectors[] = {
        {"empty", "/titles", {}, "8sK3xtqdFQ"},
        {"one", "/manga", {{"limit", "50"}}, "8qEkqsbjQMrPN82PZEAP"},
        {"sorted", "/titles", {{"keyword", "naruto"}, {"limit", "50"}, {"page", "1"}}, "8sK3xtqdFZfetBhus6bRAjHyEL3W3G_rnxgOXpPB0_i8tncqWLs"},
        {"apiStrip", "/api/titles", {{"page", "2"}}, "8sK3xtqdFZfnfxImVdE"},
        {"utf8", "/search", {{"q", "はじめの一歩"}}, "8rmP5f_9tOdgg-j3a5w_WQte2iUPEj6pvQYSPw"},
    };
    for (const auto& v : vectors) {
        CAPTURE(v.name);
        const auto got = plugin::vrf_sign(v.path, v.params, stages(), err);
        REQUIRE(got.has_value());
        CHECK(*got == v.want);
    }
}

TEST_CASE("vrf invalid stage data fails cleanly") {
    std::string err;
    // bad base64 key
    auto got = plugin::vrf_sign("/titles", {}, {{0x5A, "!!!", VRF_T1}}, err);
    CHECK_FALSE(got.has_value());
    CHECK(err.find("stage 1: key") != std::string::npos);
    // bad base64 tbl
    err.clear();
    got = plugin::vrf_sign("/titles", {}, {{0x5A, VRF_K1, "!!!"}}, err);
    CHECK_FALSE(got.has_value());
    CHECK(err.find("stage 1: tbl") != std::string::npos);
    // shrunk tbl (<256 bytes)
    err.clear();
    got = plugin::vrf_sign("/titles", {}, {{0x5A, VRF_K1, "aGVsbG8="}}, err);
    CHECK_FALSE(got.has_value());
    CHECK(err.find("tbl") != std::string::npos);
    // empty api path
    err.clear();
    got = plugin::vrf_sign("", {}, stages(), err);
    CHECK_FALSE(got.has_value());
    CHECK(err.find("api_path") != std::string::npos);
    // empty stages
    err.clear();
    got = plugin::vrf_sign("/titles", {}, {}, err);
    CHECK_FALSE(got.has_value());
    CHECK(err.find("stages") != std::string::npos);
}

TEST_CASE("vrf sign is deterministic and stage order matters") {
    std::string err;
    const auto a = plugin::vrf_sign("/titles", {{"limit", "50"}}, stages(), err);
    const auto b = plugin::vrf_sign("/titles", {{"limit", "50"}}, stages(), err);
    REQUIRE(a.has_value());
    REQUIRE(b.has_value());
    CHECK(*a == *b);
    const auto reversed = plugin::vrf_sign("/titles", {{"limit", "50"}}, {stages()[2], stages()[1], stages()[0]}, err);
    REQUIRE(reversed.has_value());
    CHECK(*reversed != *a);
}
