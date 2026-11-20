// Host-side logic check for the V guest before the daemon gate.
import { readFileSync } from 'node:fs';

const bytes = readFileSync(new URL('./main.wasm', import.meta.url));
const { instance } = await WebAssembly.instantiate(bytes, {
  env: {
    // mirror src/plugin host semantics closely enough for the template
    text_trim: (p, n, d, cap) => {
      const m = new Uint8Array(instance.exports.memory.buffer);
      let a = 0, b = n - 1;
      const ws = c => c === 32 || (c >= 9 && c <= 13);
      while (a < n && ws(m[p + a])) a++;
      while (b >= a && ws(m[p + b])) b--;
      const len = b - a + 1;
      if (len > cap) return -2;
      for (let i = 0; i < len; i++) m[d + i] = m[p + a + i];
      return len;
    },
    url_encode: (p, n, d, cap) => {
      const m = new Uint8Array(instance.exports.memory.buffer);
      const hex = '0123456789ABCDEF';
      let o = 0;
      const put = c => { if (o >= cap) return false; m[d + o++] = c; return true; };
      for (let i = 0; i < n; i++) {
        const c = m[p + i];
        const alnum = (c >= 48 && c <= 57) || (c >= 65 && c <= 90) || (c >= 97 && c <= 122) || '-_.~'.includes(String.fromCharCode(c));
        if (alnum) { if (!put(c)) return -2; }
        else { if (!put(37) || !put(hex.charCodeAt(c >> 4)) || !put(hex.charCodeAt(c & 15))) return -2; }
      }
      return o;
    },
  },
});
const E = instance.exports;
const enc = new TextEncoder();
const call = (obj) => {
  const raw = enc.encode(obj);
  const p = E.alloc(raw.length);
  new Uint8Array(E.memory.buffer).set(raw, p);
  const r = E.handle(p, raw.length);
  const n = E.last_len();
  return new TextDecoder().decode(new Uint8Array(E.memory.buffer).slice(r, r + n));
};

const t1 = JSON.parse(call('{"api":1,"method":"search","params":{"title":"  Hello V  "}}'));
console.log('search :', JSON.stringify(t1));
const t2 = JSON.parse(call('{"api":1,"method":"detail","params":{"id":"abc"}}'));
console.log('detail :', JSON.stringify(t2));
const t3 = JSON.parse(call('{"api":1,"method":"nope"}'));
console.log('unknown:', JSON.stringify(t3));
const t4 = JSON.parse(call('{"api":1,"method":"search","params":{"title":"   "}}'));
console.log('empty  :', JSON.stringify(t4));

const ok =
  t1.ok === true && t1.data.items[0].title === 'V guest: Hello V' &&
  t1.data.echo.trimmed === 'Hello V' && t1.data.echo.url_encoded === 'Hello%20V' &&
  t2.ok === true && t2.data.id === 'abc' && t2.data.tags[0] === 'v' &&
  t3.ok === false && t4.ok === true && t4.data.echo.trimmed === '';
console.log(ok ? 'LOGIC OK' : 'LOGIC FAIL');
