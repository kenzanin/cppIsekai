const util = require("./lib/util");
function handle(env) {
  if (env.method === "ping") {
    return { ok: true, data: { doubled: util.double(21), msg: util.MSG } };
  }
  if (env.method === "cfg") {
    return {
      ok: true,
      data: { v: host.config.get("str"), n: host.config.get("num"), miss: host.config.get("absent") },
    };
  }
  return { ok: false, error: "unknown method" };
}
