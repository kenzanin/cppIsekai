// MangaTown (m.mangatown.com) — JS pilot port of goIsekai examples/plugins/lua/mangatown.
// Envelope {ok,data|error}; desktop search.php scrape, mobile detail/chapters,
// desktop chapterfun assembly (2 images/request, Dean-Ewards unpack).
// Pages are plain URL strings: the reader/api consumers keep string elements
// only, and the host fetch is chrome-impersonated (no per-image headers).

// get_body parity: "" on any failure (transport, non-200, malformed reply).
function get_body(url, headers) {
  const body = host.http.fetch_body(url, headers ? { headers: headers } : undefined);
  return typeof body === "string" ? body : "";
}

// host.re2.find returns [full, g1, ...] or []; safe group read.
function grp(m, i) {
  return m && m.length > i ? m[i] : "";
}

function decode_id(p) {
  if (typeof p === "string") return p;
  const v = p && p.id;
  return typeof v === "string" ? v : "";
}

// ─── search ────────────────────────────────────────────────────────────────
// GET https://www.mangatown.com/search.php?name={q}&page={N} — desktop site
// only; the mobile search path renders nothing. Pass 1: cover rows; pass 2:
// title rows (covers may be lazy/absent); numeric redirect slugs skipped.
function search_manga(p) {
  const query = p && (p.title || p.query || p.q); // house: title first (parity lua)
  if (typeof query !== "string" || query === "") {
    return { ok: true, data: { items: [], total: 0 } };
  }
  const page = Number(p && p.page) > 0 ? Math.floor(Number(p.page)) : 1;
  const url =
    "https://www.mangatown.com/search.php?name=" + host.url_encode(query) + "&page=" + page;
  const html = get_body(url);
  const items = [];
  const seen = {};
  const add = (href, title, cover) => {
    const slug = typeof href === "string" ? href.slice("/manga/".length, -1) : "";
    if (!slug || !isNaN(Number(slug)) || seen[slug]) return;
    seen[slug] = 1;
    items.push({
      id: slug,
      name: host.text.trim(host.text.unescape(title || "")),
      cover: host.text.unescape(cover || ""),
    });
  };
  for (const m of host.re2.find_all(
    '<a class="manga_cover" href="(/manga/[^"/]+/)" title="([^"]+)">\\s*<img[^>]*src="([^"]+)"',
    html
  )) {
    add(m[1], m[2], m[3]);
  }
  for (const m of host.re2.find_all('href="(/manga/[^"/]+/)" title="([^"]+)"', html)) {
    add(m[1], m[2], "");
  }
  return { ok: true, data: { items: items, total: items.length } };
}

// ─── detail ────────────────────────────────────────────────────────────────
// GET https://m.mangatown.com/manga/{id}/ — mobile page carries title, cover,
// author, status, genres, summary ("show" span full, "hide" teaser).
function get_manga_detail(p) {
  const id = decode_id(p);
  if (id === "") return { ok: true, data: {} };
  const html = get_body("https://m.mangatown.com/manga/" + id + "/");
  if (html === "") return { ok: true, data: { id: id } }; // bare id: retried next call
  const title = host.text.trim(host.text.unescape(grp(host.re2.find('class="title">([^<]+)<', html), 1)));
  const cover = host.text.unescape(grp(host.re2.find('<img src="([^"]+)"[^>]*class="detail-cover"', html), 1));
  const author = host.text.strip_html(grp(host.re2.find('Author\\(s\\):\\s*</span>(.*?)</p>', html), 1));
  const status = host.text.normalize_status(grp(host.re2.find('Status:\\s*</span>\\s*([^<]+)', html), 1));
  const summary =
    grp(host.re2.find('(?s)<span id="show"[^>]*>(.*?)</span>', html), 1) ||
    grp(host.re2.find('(?s)<span id="hide">(.*?)</span>', html), 1);
  const genre_block = grp(host.re2.find('(?s)Genre\\(s\\):(.*?)</p>', html), 1);
  const tags = [];
  for (const m of host.re2.find_all('<a[^>]*title="[^"]*">(.*?)</a>', genre_block)) {
    const name = host.text.strip_html(m[1]);
    if (name !== "") tags.push(name);
  }
  const authors = author
    .split(",")
    .map((s) => host.text.trim(s))
    .filter((s) => s !== "");
  return {
    ok: true,
    data: {
      id: id,
      name: title,
      cover: cover,
      status: status,
      tags: tags,
      release_at: 0,
      description: host.text.strip_html(summary),
      authors: authors,
      artists: [],
      url: "https://m.mangatown.com/manga/" + id + "/",
    },
  };
}

// ─── chapters ──────────────────────────────────────────────────────────────
const MONTHS = { Jan: 0, Feb: 1, Mar: 2, Apr: 3, May: 4, Jun: 5, Jul: 6, Aug: 7, Sep: 8, Oct: 9, Nov: 10, Dec: 11 };
// "Jul 22,2014" (mobile chapter <span class=time>) -> epoch seconds; 0 if absent
function ts_of(s) {
  const m = /^([A-Z][a-z]{2}) (\d{1,2}),(\d{4})$/.exec((s || "").trim());
  if (!m) return 0;
  const t = Date.UTC(+m[3], MONTHS[m[1]], +m[2]) / 1000;
  return isFinite(t) ? t : 0;
}

// Same detail page. Links: /manga/{slug}/v09/c011.5/ or /manga/{slug}/c070/.
// Decimal-safe: c([0-9][0-9.]*)$ anchored at path end (c011.5 AND c012.1).
// Ascending order = cppIsekai reading-order convention (host re-sorts too).
function get_chapter_list(p) {
  const id = decode_id(p);
  const out = [];
  if (id !== "") {
    const html = get_body("https://m.mangatown.com/manga/" + id + "/");
    const seen = {};
    if (html !== "") {
      const pat =
        '(?s)href="/manga/' + host.re2.quote(id) + '/((?:v[0-9]+/)?c[0-9.]+)/".*?class="time">([^<]+)<';
      for (const m of host.re2.find_all(pat, html)) {
        const rest = m[1];
        const num = parseFloat(grp(host.re2.find("c([0-9][0-9.]*)$", rest), 1));
        if (isNaN(num) || seen[rest]) continue;
        seen[rest] = 1;
        out.push({
          id: id + "/" + rest,
          number: num,
          title: "Chapter " + num,
          release_at: ts_of(m[2]),
          pages: 0,
        });
      }
      out.sort((a, b) => a.number - b.number);
    }
  }
  return { ok: true, data: out };
}

// ─── pages ─────────────────────────────────────────────────────────────────
// Desktop chapterfun path: 2 images per request (a 34-page mobile chapter is
// N round-trips). Mobile fallback scrapes <img id="image"> per page.
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

// Host seam: envelope dispatch, parity with the lua plugins' handle().
function handle(req) {
  const p = (req && req.params) || {};
  switch (req && req.method) {
    case "search":
      return search_manga(p);
    case "detail":
      return get_manga_detail(p);
    case "chapters":
      return get_chapter_list(p);
    case "pages":
    case "genres":
      return { ok: true, data: [
        "4 Koma", "Action", "Adventure", "Comedy", "Cooking", "Doujinshi", "Drama", "Ecchi", "Fantasy", "Gender Bender", "Harem", "Historical", "Horror", "Martial Arts", "Mature", "Mecha", "Music", "Mystery", "One Shot", "Psychological", "Reverse Harem", "Romance", "School Life", "Sci Fi", "Shotacon", "Slice Of Life", "Smut", "Sports", "Supernatural", "Suspense", "Tragedy", "Vampire", "Webtoons", "Youkai",
      ] };
      return get_page_list(p);
    default:
      return { ok: false, error: "unknown method " + ((req && req.method) || "") };
  }
}
