module main

// vexample -- minimal cppIsekai wasm guest written in V (SSA backend).
//
// ABI (src/plugin/wasm/wasm_rt.hpp): the host writes the JSON envelope
//   {"api":1,"method":"search","params":{"title":".."}} / {"..":"..","id":".."}
// at alloc(len) and calls handle(ptr,len); the reply is read from the returned
// pointer with last_len() bytes and must parse as JSON:
//   {"ok":true,"data":..}  /  {"ok":false,"error":".."}
// Host services are env.* imports (src/plugin/wasm/wasm_imports.cpp); this
// guest calls text_trim and url_encode to show the seam.
//
// Template plugin, not a site port: `search` returns one synthetic item built
// from the query so the whole path (route -> runtime -> imports -> reply
// parse -> UI) is exercised without scraping anything.
//
// Build: ./build.sh   (v -b wasm; no C toolchain, no wasi-sdk)
//
// V wasm-backend gotchas baked into this file (probes in git history):
// - __global ARRAYS get one hidden pointer slot each and every slot is
//   zero-initialized, so two arrays collapse onto address 0. Use ONE array
//   plus offset consts; never rely on distinct __global arrays.
// - `arr[i] = other_arr[j]` (global<-global element store) drops the store;
//   route the value through a local temp.
// - a zeroing loop directly before the copy loop into the SAME array drops
//   the copy's stores; write a terminator instead of pre-zeroing.
// - `&arr` yields the hidden slot address, `&arr[0]` the data pointer; only
//   the latter is a valid ABI address.

@[wasm_import_namespace: 'env']
fn ENV.text_trim(src int, src_len int, dst int, dst_cap int) int

@[wasm_import_namespace: 'env']
fn ENV.url_encode(src int, src_len int, dst int, dst_cap int) int

// single backing array; regions laid out by offset (see gotchas above)
const in_off = 4096 // envelope; head kept clear of the rodata/data segment
const val_cap = 8192
const val_off = 69632 // 4096 + 65536 (max envelope)
const trim_off = 77824
const enc_off = 86016
const meth_cap = 32
const meth_off = 94208
const out_cap = 4096
const out_off = 94240
const a_cap = 98336 // out_off + out_cap

__global abuf [98336]u8
__global out_len u32
__global op int      // reply write cursor (abuf-relative)
__global env_len int // envelope length; bounds every scan

fn abase() int {
	return int(&abuf[0])
}

fn aptr(off int) int {
	return abase() + off
}

pub fn alloc(n int) int {
	if n > 65536 {
		return 0
	}
	return aptr(in_off)
}

pub fn last_len() u32 {
	return out_len
}

// json_str_value scans the envelope for "key":"value" and copies value into
// the val region. Returns the value length, 0 when absent. Flat string fields
// only -- every envelope field this guest reads is one.
fn json_str_value(key string) int {
	klen := key.len
	limit := env_len
	mut i := 0
	for i + 4 + klen < limit {
		mut k := 0
		for k < klen && abuf[in_off + i + 1 + k] == key[k] {
			k++
		}
		// key's closing quote at i+klen+1, then ':' then the value quote
		if k == klen && abuf[in_off + i] == 34 && abuf[in_off + i + klen + 1] == 34
			&& abuf[in_off + i + klen + 2] == 58 && abuf[in_off + i + klen + 3] == 34 {
			mut j := i + klen + 4
			mut n := 0
			for j < limit && abuf[in_off + j] != `"` {
				if n < val_cap {
					abuf[val_off + n] = abuf[in_off + j]
					n++
				}
				j++
			}
			return n
		}
		i++
	}
	return 0
}

fn emit_str(s string) {
	for j in 0..s.len {
		ch := s[j]
		abuf[out_off + op] = ch
		op++
	}
}

// emit_buf JSON-escapes abuf[off..off+n] into the reply (quote, backslash;
// control codepoints dropped) -- query values are plain text, this keeps the
// envelope well-formed whatever the search box contained.
fn emit_buf(buf_off int, n int) {
	for j in 0..n {
		c := abuf[buf_off + j]
		if c == `"` {
			emit_byte(`\\`)
			emit_byte(`"`)
		} else if c == `\\` {
			emit_byte(`\\`)
			emit_byte(`\\`)
		} else if c >= 32 {
			emit_byte(c)
		}
	}
}

fn emit_byte(c u8) {
	abuf[out_off + op] = c
	op++
}

fn err_reply(msg string) {
	op = 0
	emit_str('{"ok":false,"error":"')
	emit_str(msg)
	emit_str('"}')
	out_len = u32(op)
}

fn meth_is(s string) bool {
	if s.len > meth_cap {
		return false
	}
	mut k := 0
	for k < s.len {
		if abuf[meth_off + k] != s[k] {
			return false
		}
		k++
	}
	return k < meth_cap && abuf[meth_off + k] == 0
}

pub fn handle(env int, len int) int {
	out_len = 0
	env_len = len
	if len <= 0 || len > 65536 {
		err_reply('bad envelope length')
		return aptr(out_off)
	}
	// method -> meth region (val staging). No zeroing loop: see file header.
	mut n := json_str_value('method')
	if n > meth_cap - 1 {
		n = meth_cap - 1
	}
	mut j := 0
	for j < n {
		t := abuf[val_off + j] // global<-global store needs the temp
		abuf[meth_off + j] = t
		j++
	}
	abuf[meth_off + n] = 0

	if meth_is('search') {
		n = json_str_value('title')
		if n == 0 {
			err_reply('empty query')
			return aptr(out_off)
		}
		mut t := ENV.text_trim(aptr(val_off), n, aptr(trim_off), val_cap)
		if t < 0 {
			t = 0 // host error maps to empty, per ABI note; 0 is a valid empty
		}
		e := ENV.url_encode(aptr(trim_off), t, aptr(enc_off), val_cap)
		if e < 0 {
			return err_reply_ptr('url_encode failed')
		}
		op = 0
		emit_str('{"ok":true,"data":{"items":[{"id":"v-example","title":"V guest: ')
		emit_buf(trim_off, t)
		emit_str('","cover":""}],"total":1,"echo":{"trimmed":"')
		emit_buf(trim_off, t)
		emit_str('","url_encoded":"')
		emit_buf(enc_off, e)
		emit_str('"}}}')
		out_len = u32(op)
		return aptr(out_off)
	}

	if meth_is('detail') {
		n = json_str_value('id')
		op = 0
		emit_str('{"ok":true,"data":{"id":"')
		emit_buf(val_off, n)
		emit_str('","name":"V guest: ')
		emit_buf(val_off, n)
		emit_str('","cover":"","status":"Unknown","tags":["v","wasm"],')
		emit_str('"authors":["cppIsekai"],"synopsis":"Synthetic entry from ')
		emit_str('plugins_dev/wasm/vexample - a V guest for the wasm plugin ABI."}}')
		out_len = u32(op)
		return aptr(out_off)
	}

	err_reply('unknown method')
	return aptr(out_off)
}

// err_reply_ptr mirrors err_reply for the early-return-in-else shape above.
fn err_reply_ptr(msg string) int {
	err_reply(msg)
	return aptr(out_off)
}

fn main() {
}
