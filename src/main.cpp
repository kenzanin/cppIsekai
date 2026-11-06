// Dependency smoke test: proves compile+link+run for every lib.
// ponytail: replace with the real application main.
#include <print>
#include <string>

#include <BS_thread_pool.hpp>
#include <crow.h>
#include <glaze/glaze.hpp>
#include <readerwriterqueue.h>
#include <sol/sol.hpp>

#include <curl/curl.h>
#include <sqlite3.h>
#include <vips/vips.h>
#include <wasm3.h>

struct Packet {
    int id{};
    std::string name;
};

int main() {
    std::println("cppIsekai :: dependency smoke");

    std::println("sqlite3/libsql {}", sqlite3_libversion());

    IM3Environment env = m3_NewEnvironment();
    m3_FreeEnvironment(env);
    std::println("wasm3 env ok");

    sol::state lua;
    lua.open_libraries(sol::lib::base);
    std::println("lua {}", lua["_VERSION"].get<std::string>());

    BS::thread_pool pool(2);
    pool.wait();
    std::println("thread_pool ok");

    moodycamel::ReaderWriterQueue<int> queue(8);
    queue.enqueue(42);
    int value = 0;
    queue.try_dequeue(value);
    std::println("readerwriterqueue {}", value);

    Packet p{7, "isekai"};
    std::string buf;
    (void)glz::write_json(p, buf);
    Packet q{};
    (void)glz::read_json(q, buf);
    std::println("glaze {} roundtrip={}", buf, q.id == 7);

    curl_global_init(CURL_GLOBAL_DEFAULT);
    std::println("curl-impersonate {}", curl_version());
    curl_global_cleanup();

    vips_init("cppIsekai");
    std::println("vips {}", VIPS_VERSION);
    vips_shutdown();

    crow::SimpleApp app;
    std::println("crow headers ok");
}
