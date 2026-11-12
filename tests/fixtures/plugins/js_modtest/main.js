const util = require("./lib/util");
function handle(env) {
  if (env.method === "ping") {
    return { ok: true, data: { doubled: util.double(21), msg: util.MSG } };
  }
  return { ok: false, error: "unknown method" };
}
