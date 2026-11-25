const { get_body, grp, decode_id } = require("common");
function get_page_list(p) {
  const id = decode_id(p);
  return { ok: true, data: id === "" ? [] : page_urls(id) };
}

function page_count(html, chapter_id) {
  let max_page = 1;
  for (const m of host.re2.find_all("/manga/" + host.re2.quote(chapter_id) + "/([0-9]+)\\.html", html)) {
    const v = parseInt(m[1], 10);
    if (v > max_page) max_page = v;
  }
  return max_page;
}

function page_image(html) {
  const tag = grp(host.re2.find('<img[^>]*id="image"[^>]*>', html), 0);
  let src = host.text.unescape(grp(host.re2.find('src="([^"]+)"', tag), 1));
  if (src.startsWith("//")) src = "https:" + src;
  return src;
}

// Packed blob trailing args: }('{payload}',{base},{len},'{words}'.split('|'),0,{})
function unpack_packer(packed) {
  const m = host.re2.find(
    "(?s)\\}\\('(.*)',(\\d+),\\d+,'(.*)'\\.split\\('\\|'\\),0,\\{\\}\\)",
    packed
  );
  if (m.length < 4) return null;
  const payload = m[1];
  const base = parseInt(m[2], 10);
  const words = m[3].split("|"); // JS split keeps the empties lua coded around
  // base-N index encoder (Dean-Edwards e fn): 0-9, a-z (36), A-Z (c+29).
  const enc = (x, n) => {
    let digits = "";
    for (;;) {
      const r = x % n;
      const c = r < 10 ? String(r) : r < 36 ? String.fromCharCode(87 + r) : String.fromCharCode(29 + r);
      digits = c + digits;
      x = Math.floor(x / n);
      if (x === 0) break;
    }
    return digits;
  };
  const dict = {};
  words.forEach((w, i) => {
    dict[enc(i, base)] = w;
  });
  // %w+ table gsub parity: unknown tokens stay as-is.
  return payload.replace(/\w+/g, (w) => (w in dict ? dict[w] : w));
}

// "kimi_no_knife/c070" -> "0"; "kimi_no_kakera/v09/c011.5" -> "5". The packed
// pix folder truncates the decimal suffix; re-append it before joining.
function chapter_suffix(chapter_id) {
  const num = grp(host.re2.find("c([0-9][0-9.]*)$", chapter_id), 1);
  const dot = num.indexOf(".");
  return dot === -1 ? "0" : num.slice(dot + 1) || "0";
}

function page_urls(chapter_id) {
  const mob_base = "https://m.mangatown.com/manga/" + chapter_id + "/";
  const desk_base = "https://www.mangatown.com/manga/" + chapter_id + "/";
  const desk = get_body(desk_base);
  if (desk === "") return [];
  const cid = grp(host.re2.find("var chapter_id=([0-9]+);", desk), 1);
  const total = parseInt(grp(host.re2.find("var total_pages=(\\d+);", desk), 1), 10);
  if (cid === "" || isNaN(total) || total < 1) {
    // No desktop reader data: fall back to per-page mobile scraping.
    const first = get_body(mob_base + "1.html");
    if (first === "") return [];
    const pages = [];
    const count = page_count(first, chapter_id);
    for (let i = 1; i <= count; i++) {
      const html = i === 1 ? first : get_body(mob_base + i + ".html");
      const src = page_image(html);
      if (src !== "") pages.push(src);
    }
    return pages;
  }
  const suffix = chapter_suffix(chapter_id);
  const seen = {};
  const urls = [];
  for (let k = 1; urls.length < total && k <= total; k += 2) {
    const packed = get_body(
      "https://www.mangatown.com/chapterfun.ashx?cid=" + cid + "&page=" + k + "&key=",
      { Referer: desk_base }
    );
    if (packed === "") break;
    const js = unpack_packer(packed);
    if (js === null) break;
    // Re-append the truncated decimal folder suffix, then join.
    const pix = grp(host.re2.find('pix="([^"]+)"', js), 1) + suffix;
    for (const m2 of host.re2.find_all('"(/[^"]+\\.jpg)"', js)) {
      const fname = m2[1];
      if (!seen[fname]) {
        seen[fname] = 1;
        urls.push("https:" + pix + fname);
      }
    }
  }
  return urls;
}
module.exports = { page_urls, get_page_list };
