// Minify custom static js + brotli q11 -> <name>.js.br. Sources untouched.
// Minifier: uglify-js via `bunx --package uglify-js` (already in bun global store;
// bun build/Bun.build tree-shakes unexported top-level decls -> empty bundle, wrong
// shape for classic <script src> globals). -m keeps top-level names (toplevel:false),
// --keep-fnames keeps every fn name for inline-template/sibling callers.
// ponytail: swap to esbuild --minify for ~15% smaller output if ever needed.
import { brotliCompressSync, constants } from "node:zlib";
import { readdirSync, statSync, writeFileSync } from "node:fs";
import path from "node:path";

let n = 0;
for (const f of readdirSync("static")) {
  if (!f.endsWith(".js") || f.includes(".min.")) continue;
  const src = path.join("static", f);
  const p = Bun.spawnSync(["bunx", "--package", "uglify-js", "uglifyjs", src, "-c", "-m", "--keep-fnames"], {
    stdout: "pipe",
    stderr: "inherit",
  });
  if (!p.success || p.exitCode !== 0) {
    console.error(`${f}: uglify failed (exit ${p.exitCode})`);
    process.exit(1);
  }
  const min = p.stdout.toString();
  const br = brotliCompressSync(Buffer.from(min), {
    params: { [constants.BROTLI_PARAM_QUALITY]: 11 },
  });
  writeFileSync(`${src}.br`, br);
  console.log(`${f}: ${statSync(src).size}B -> min ${Buffer.byteLength(min)}B -> br ${br.length}B`);
  n++;
}
console.log(`${n} asset(s) regenerated`);
