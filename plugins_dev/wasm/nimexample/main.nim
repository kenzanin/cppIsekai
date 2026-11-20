# nimexample — minimal cppIsekai wasm guest written in Nim (wasi-sdk target).
#
# ABI (src/plugin/wasm/wasm_rt.hpp): the host writes a JSON envelope
#   {"api":1,"method":..,"params":{..}}
# at alloc(len) and calls handle(ptr,len); the reply is read from the returned
# pointer with last_len() bytes and must parse as JSON:
#   {"ok":true,"data":..}  /  {"ok":false,"error":".."}
# Host services are env.* imports (src/plugin/wasm/wasm_imports.cpp); this
# guest calls text_trim and url_encode to show the seam.
#
# This plugin is a template, not a site port: `search` returns one synthetic
# item built from the query so the whole path (route -> runtime -> imports ->
# reply parse -> UI) can be exercised without scraping anything.
#
# Build: ./build.sh  (Nim -> objects, wasi-sdk clang -> main.wasm)

import std/json
import std/parsejson
import std/streams

const Cap = 1 shl 16

var inbuf: array[Cap, char]     # host writes the envelope here
var reply: string               # must outlive handle() — host reads it after return
var gLastLen: uint32

proc hostTextTrim(src: uint32; srcLen: uint32; dst: uint32; dstLen: uint32): int32 {.
    importc: "text_trim", cdecl.}
proc hostUrlEncode(src: uint32; srcLen: uint32; dst: uint32; dstLen: uint32): int32 {.
    importc: "url_encode", cdecl.}

proc addrOf(s: var string): uint32 =
  if s.len == 0: 0'u32 else: cast[uint32](cast[uint](s[0].addr))

proc byteAddr(p: uint32): pointer = cast[pointer](cast[uint](p))

var scratch: array[1 shl 15, char]

proc call1(f: proc(a: uint32; b: uint32; c: uint32; d: uint32): int32 {.cdecl.}; s: string): string =
  if s.len == 0: return ""
  let n = f(cast[uint32](cast[uint](unsafeAddr s[0])), uint32(s.len),
            cast[uint32](cast[uint](addr scratch[0])), uint32(scratch.len - 1))
  if n <= 0: return ""
  result = newString(int(n))
  copyMem(addr result[0], addr scratch[0], int(n))

# Envelope fields via std/parsejson (the lexer layer under std/json).
# nim 2.4.2 note: the convenient parseJson() SIGSEGVs the compiler when
# reachable from an exportc proc with --os:standalone --cpu:wasm32, so the
# event walker below is used instead; %* / $ for building replies are fine.
proc envStr(env, key: string): string =
  # next() is an event API: ':' / ',' never surface, so key-vs-value is tracked
  # here -- after '{' or a ',' in an object the next string is a key.
  var ss = newStringStream(env)
  var p: JsonParser
  p.open(ss, "env")
  var stack: seq[bool]   # true = object, false = array
  var pendingKey = ""
  var phaseValue = false # true: last string was a key, next token is its value
  while true:
    p.next()
    case p.tok
    of tkEof: break
    of tkCurlyLe:
      stack.add true; phaseValue = false
    of tkBracketLe:
      stack.add false; phaseValue = false
    of tkCurlyRi, tkBracketRi:
      if stack.len > 0: discard stack.pop
      phaseValue = false
    of tkComma:
      phaseValue = stack.len > 0 and stack[stack.high]
    of tkString:
      if not phaseValue and (stack.len == 0 or stack[stack.high]):
        pendingKey = p.str
        phaseValue = true
      else:
        if pendingKey == key: p.close(); return p.str
        phaseValue = false
    else:
      phaseValue = false # int/true/... value consumed the pending key
  p.close()
  return ""

proc jesc(s: string): string =
  result = ""
  for ch in s:
    case ch
    of '"': result.add "\\\""
    of '\\': result.add "\\\\"
    of '\n': result.add "\\n"
    of '\r': result.add "\\r"
    of '\t': result.add "\\t"
    else:
      if uint8(ch) >= 0x20: result.add ch

proc errReply(msg: string) =
  reply = "{\"ok\":false,\"error\":\"" & jesc(msg) & "\"}"

# ── entry ────────────────────────────────────────────────────────────────
proc alloc*(len: uint32): uint32 {.exportc: "alloc", cdecl.} =
  let n = if len == 0: 4096'u32 else: len
  if n > uint32(Cap): return 0
  cast[uint32](cast[uint](addr inbuf[0]))

proc handle*(envPtr, len: uint32): uint32 {.exportc: "handle", cdecl.} =
  if envPtr == 0 or len > uint32(Cap):
    errReply("bad envelope pointer")
  else:
    var env = newString(int(len))
    copyMem(addr env[0], byteAddr(envPtr), int(len))
    let meth = envStr(env, "method")
    if meth == "search":
      let q = call1(hostTextTrim, envStr(env, "title"))
      if q.len == 0: errReply("empty query")
      else:
        let enc = call1(hostUrlEncode, q)
        reply = "{\"ok\":true,\"data\":{\"items\":[{\"id\":" &
                "\"nim-example\",\"title\":\"Nim guest: " & jesc(q) & "\"," &
                "\"cover\":\"\"}],\"total\":1,\"echo\":{\"trimmed\":\"" & jesc(q) &
                "\",\"url_encoded\":\"" & jesc(enc) & "\"}}}"
    elif meth == "detail":
      let id = envStr(env, "id")
      reply = "{\"ok\":true,\"data\":{\"id\":\"" & jesc(id) &
              "\",\"name\":\"Nim guest: " & jesc(id) & "\",\"cover\":\"\"," &
              "\"status\":\"Unknown\",\"tags\":[\"nim\",\"wasm\"]," &
              "\"authors\":[\"cppIsekai\"]," &
              "\"synopsis\":\"Synthetic entry from plugins_dev/wasm/nimexample — " &
              "a Nim guest for the wasm plugin ABI.\"}}"
    else:
      errReply("unknown method: " & meth)
  gLastLen = uint32(reply.len)
  cast[uint32](cast[uint](addrOf(reply)))

proc last_len*(): uint32 {.exportc: "last_len", cdecl.} = gLastLen
