// Fixture: happy path + budget spin, driven from tests/test_plugin.cpp.
function handle(env) {
  if (env.method === "ping") {
    return { ok: true, data: { js: "pong" } };
  }
  if (env.method === "spin") {
    while (true) {} // interrupt handler ends the call: ok:false
  }
  if (env.method === "url") {
    const s = "a b&c=d%?";
    return {
      ok: true,
      data: {
        enc: host.url_encode(s),
        dec: host.url_decode("a%20b%26c%3Dd%25%3F"),
        rt: host.url_decode(host.url_encode(env.params.s)),
      },
    };
  }
  if (env.method === "urlbad") {
    try {
      host.url_encode(42); // non-string arg: TypeError per host surface
      return { ok: false, error: "url_encode accepted a number" };
    } catch (e) {
      return { ok: true, data: { msg: String(e) } };
    }
  }
  return { ok: false, error: "unknown method" };
}
