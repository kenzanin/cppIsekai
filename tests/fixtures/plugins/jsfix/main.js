// Fixture: happy path + budget spin, driven from tests/test_plugin.cpp.
function handle(env) {
  if (env.method === "ping") {
    return { ok: true, data: { js: "pong" } };
  }
  if (env.method === "spin") {
    while (true) {} // interrupt handler ends the call: ok:false
  }
  return { ok: false, error: "unknown method" };
}
