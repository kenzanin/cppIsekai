package odinexample

// Odin guest for the cppIsekai wasm plugin ABI — mirrors plugins_dev/wasm/nimexample.
// ABI: host writes the JSON request at alloc(len), calls handle(ptr,len), reads the
// reply (must parse as {"ok":..}) from the returned pointer for last_len() bytes.
//
// Built runtime-init-free on purpose: wasm3 never runs _start (no start section in
// this module), so handle() touches no global runtime state — parse scratch comes
// from an arena carved out of static buffers, replies are written straight into a
// static out buffer.

import "base:runtime"
import "core:encoding/json"
import "core:mem"
import "core:strings"

IN_CAP :: 1 << 16 // request envelope
OUT_CAP :: 1 << 17 // reply
SCR_CAP :: 1 << 16 // host-call scratch

in_buf: [IN_CAP]u8
out_buf: [OUT_CAP]u8
scratch_a: [SCR_CAP]u8 // text_trim result
scratch_b: [SCR_CAP]u8 // url_encode result
arena_buf: [1 << 16]u8 // json parse arena

g_out_len: u32

foreign import "env"

foreign env {
	text_trim :: proc "c" (src, src_len, dst, dst_cap: u32) -> i32 ---
	url_encode :: proc "c" (src, src_len, dst, dst_cap: u32) -> i32 ---
}

addr_u32 :: proc "contextless" (p: rawptr) -> u32 {
	return cast(u32)cast(uintptr)p
}

// local aliases for the builder writers
write_byte :: strings.write_byte
write_string :: strings.write_string

host_trim :: proc(s: string) -> []u8 {
	if len(s) == 0 do return scratch_a[0:0]
	n := text_trim(addr_u32(raw_data(s)), u32(len(s)), addr_u32(&scratch_a[0]), u32(len(scratch_a) - 1))
	if n <= 0 do return scratch_a[0:0]
	return scratch_a[0:n]
}

host_encode :: proc(s: []u8) -> []u8 {
	if len(s) == 0 do return scratch_b[0:0]
	n := url_encode(addr_u32(raw_data(s)), u32(len(s)), addr_u32(&scratch_b[0]), u32(len(scratch_b) - 1))
	if n <= 0 do return scratch_b[0:0]
	return scratch_b[0:n]
}

// envelope value lookup: top level, then one level down inside "params"
field_string :: proc(obj: json.Object, key: string) -> string {
	if v, ok := obj[key]; ok {
		if s, ok2 := v.(json.String); ok2 do return s
	}
	if pv, ok := obj["params"]; ok {
		if pm, ok2 := pv.(json.Object); ok2 {
			if v2, ok3 := pm[key]; ok3 {
				if s2, ok4 := v2.(json.String); ok4 do return s2
			}
		}
	}
	return ""
}

json_escape_to :: proc(b: ^strings.Builder, s: string) {
	for i in 0 ..< len(s) {
		c := s[i]
		if c == '"' {
			write_byte(b, '\\')
			write_byte(b, '"')
		} else if c == '\\' {
			write_byte(b, '\\')
			write_byte(b, '\\')
		} else if c == '\n' {
			write_string(b, "\\n")
		} else if c == '\r' {
			write_string(b, "\\r")
		} else if c == '\t' {
			write_string(b, "\\t")
		} else if c < 0x20 {
			continue
		} else {
			write_byte(b, c)
		}
	}
}

emit_error :: proc(msg: string) -> u32 {
	b := strings.builder_from_bytes(out_buf[:])
	write_string(&b, `{"ok":false,"error":"`)
	json_escape_to(&b, msg)
	write_string(&b, `"}`)
	g_out_len = u32(strings.builder_len(b))
	return addr_u32(&out_buf[0])
}

@(export)
alloc :: proc "c" (req: u32) -> u32 {
	n := req
	if n == 0 do n = 4096
	if int(n) > len(in_buf) do return 0
	return addr_u32(&in_buf[0])
}

handle_impl :: proc(ptr, env_len: u32) -> u32 {
	if ptr == 0 || int(env_len) > len(in_buf) {
		return emit_error("bad envelope pointer")
	}

	prev := context
	arena: mem.Arena
	mem.arena_init(&arena, arena_buf[:])
	ctx := context
	ctx.allocator = mem.arena_allocator(&arena)
	context = ctx
	defer context = prev

	v, err := json.parse_bytes(in_buf[0:int(env_len)])
	if err != nil do return emit_error("envelope parse failed")
	obj, ok := v.(json.Object)
	if !ok do return emit_error("envelope not an object")

	meth := field_string(obj, "method")
	b := strings.builder_from_bytes(out_buf[:])

	switch meth {
	case "search":
		q := host_trim(field_string(obj, "title"))
		if len(q) == 0 {
			return emit_error("empty query")
		}
		enc := host_encode(q)

		write_string(&b, `{"ok":true,"data":{"items":[{"id":"odin-example","title":"Odin guest: `)
		json_escape_to(&b, string(q))
		write_string(&b, `","cover":""}],"total":1,"echo":{"trimmed":"`)
		json_escape_to(&b, string(q))
		write_string(&b, `","url_encoded":"`)
		json_escape_to(&b, string(enc))
		write_string(&b, `"}}}`)
	case "detail":
		id := field_string(obj, "id")
		write_string(&b, `{"ok":true,"data":{"id":"`)
		json_escape_to(&b, id)
		write_string(&b, `","name":"Odin guest: `)
		json_escape_to(&b, id)
		write_string(&b, `","cover":"","status":"Unknown","tags":["odin","wasm"],"authors":["cppIsekai"],"synopsis":"Synthetic entry from plugins_dev/wasm/odinexample - an Odin guest for the wasm plugin ABI."}}`)

	case:
		eb := strings.builder_from_bytes(out_buf[:])
		write_string(&eb, `{"ok":false,"error":"unknown method: `)
		json_escape_to(&eb, meth)
		write_string(&eb, `"}`)
		g_out_len = u32(strings.builder_len(eb))
		return addr_u32(&out_buf[0])
	}

	g_out_len = u32(strings.builder_len(b))
	return addr_u32(&out_buf[0])
}

@(export)
handle :: proc "c" (ptr, env_len: u32) -> u32 {
	context = runtime.default_context()
	return handle_impl(ptr, env_len)
}

@(export)
last_len :: proc "c" () -> u32 {
	return g_out_len
}

main :: proc() {}
