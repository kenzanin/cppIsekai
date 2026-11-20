# Odin Example — wasm guest in Odin

Mirror of `../nimexample` written in Odin: proves the guest ABI (alloc/handle/last_len,
JSON envelope in/out) and the two host imports `env.text_trim` / `env.url_encode` from
Odin. Search echoes the trimmed query plus its url-encoded form; detail returns a
synthetic entry. Not a real site scraper — see the Lua plugins under `plugins_dev/lua/`.

## Build

./build.sh   # -> main.wasm

## Pin: odin dev-2026-10:84bc3fc21

Recheck the traps below when bumping; they were all paid for on this version.

## Traps learned (2026-11-20, odin dev-2026-10:84bc3fc21)

- Target is `wasi_wasm32` (`odin build . -target:wasi_wasm32`). `freestanding_wasm32`
  choked on a plain hello world; not needed anyway.
- Exports: `@(export)` (boolean) on a `proc "c"`; the wasm symbol is the proc name, so
  name the procs `alloc`/`handle`/`last_len` exactly.
- Host imports: `foreign import "env"` + a `foreign env { ... --- }` block; procedures
  inside a foreign block need the trailing `---` (body-less declaration).
- `proc "c"` has NO implicit `context`. Any call into a default-convention proc from a
  `"c"` proc fails with "context has not been defined within this scope". Fix: keep
  `@(export)` procs as one-line wrappers that first do `context = default_context()`
  (import `base:runtime`) and delegate to a normal proc holding the real body.
- wasm3 (host) never runs `_start`: the module has no start section, so the odin
  runtime (`__$startup_runtime`) is NOT executed by the host. Keep the guest
  runtime-init-free: no allocator use outside an arena you carve yourself, replies
  written into static buffers only. `handle` swaps `context` to an arena over a
  static buffer for the JSON parse and restores it on exit.
- JSON: `core:encoding/json` `parse_bytes` works fine here (unlike Nim's std/json
  segfault); feed it `context.allocator` = the arena. Building the reply:
  `strings.builder_from_bytes(out_buf[:])` — a no-alloc builder over the static reply
  buffer; `write_string`/`write_byte` into it, `builder_len` for the size.
- `string([]u8)` conversion is legal (copies); `&s[0]` on a string param is not — use
  `raw_data(s)` for the pointer, `cast(u32)cast(uintptr)p` for the wasm address.
- Runtime string `+` concatenation is a compile error ("only allowed with constant
  strings") — build replies piecewise through the builder instead.
- `switch` is the keyword; `case x:` labels inside, bare `case:` = default.
- Register with the daemon: symlink `data/plugins/odinexample ->
  ../../plugins_dev/wasm/odinexample`, restart, and purge `api_cache` rows
  (`plugin_id='odinexample'`) after any reply-shape change.
