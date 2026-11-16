// Mangzio (mangzio.com) — JS port of goIsekai examples/plugins/js/mangzio.
// Next.js RSC flight-data scraping; envelope {ok,data|error} via handle().
// Pages are plain URL strings; chapter ids "slug:chapter-N". Search is a
// client-side filter over the SSR catalog (site has no server search).

const BASE = "https://www.mangzio.com";

// get_body parity: "" on any failure (transport, non-200, malformed reply).
function get_body(url) {
  const body = host.http.fetch_body(url);
  return typeof body === "string" ? body : "";
}

// RSC flight chunks: payload strings of self.__next_f.push([1,"..."]),
// unicode escapes folded to characters (parity with the goIsekai original).
function rsc_chunks(html) {
  const chunks = [];
  for (const m of host.re2.find_all(
    'self\\.__next_f\\.push\\(\\[1,"(.+?)"\\]\\)',
    html
  )) {
    let s = m[1].replace(/\\u([0-9a-fA-F]{4})/g, (_, h) =>
      String.fromCharCode(parseInt(h, 16))
    );
    s = s.replace(/\\(.)/g, "$1");
    chunks.push(s);
  }
  return chunks;
}

// First balanced {...} whose walk encloses `needle`; string-aware; parsed
// object or null.
function find_object(chunks, needle) {
  for (const chunk of chunks) {
    const at = chunk.indexOf(needle);
    if (at < 0) continue;
    const start = chunk.lastIndexOf("{", at);
    if (start < 0) continue;
    let depth = 0;
    let inStr = false;
    let esc = false;
    for (let i = start; i < chunk.length && i - start < 100000; i++) {
      const ch = chunk[i];
      if (inStr) {
        if (esc) esc = false;
        else if (ch === "\\") esc = true;
        else if (ch === '"') inStr = false;
        continue;
      }
      if (ch === '"') inStr = true;
      else if (ch === "{") depth++;
      else if (ch === "}") {
        depth--;
        if (depth === 0) {
          if (start < at && at < i) {
            try {
              return JSON.parse(chunk.slice(start, i + 1));
            } catch (e) {
              break;
            }
          }
          break; // closed before/at needle: probe an earlier brace? none kept
        }
      }
    }
  }
  return null;
}

// Balanced array/object value following `"key":`; string-aware; first
// parseable occurrence wins, else null.
// Array value following `"key":` via one host re2 pass (nesting depth 1);
// capture is JSON.parse'd guest-side. A char-walk balanced scan over the
// joined flight stream blows the 200k ops budget (interrupted) -- keep the
// walk in C.
function array_after(hay, key) {
  const rows = host.re2.find_all(
    key + "(\\[[^\\[\\]]*(?:\\[[^\\[\\]]*\\][^\\[\\]]*)*\\])",
    hay
  );
  if (rows.length === 0 || typeof rows[0][1] !== "string") return null;
  try {
    return JSON.parse(rows[0][1]);
  } catch (e) {
    return null;
  }
}

// ─── search ────────────────────────────────────────────────────────────────
// GET /en?q={q}; the SSR payload embeds the catalog as manga cards
// ("slug","baseSlug","title","alternativeTitle","coverImageUrl"); local
// case-insensitive substring filter stands in for the client-side search.
function search_manga(p) {
  const query = p && (p.title || p.query || p.q); // house: title first (parity lua)
  if (typeof query !== "string" || query === "") {
    return { ok: true, data: { items: [], total: 0 } };
  }
  const html = get_body(BASE + "/en?q=" + host.url_encode(query));
  const items = [];
  if (html !== "") {
    const hay = rsc_chunks(html).join("");
    const seen = {};
    for (const m of host.re2.find_all(
      '"slug":"([^"]+)","baseSlug":"[^"]*","title":"([^"]+)","alternativeTitle":(?:null|"[^"]*"),"coverImageUrl":"([^"]+)"',
      hay
    )) {
      const slug = m[1];
      if (!slug || slug === "manga" || seen[slug]) continue;
      seen[slug] = 1;
      const cover = m[3];
      items.push({
        id: slug,
        name: host.text.trim(host.text.unescape(m[2])),
        cover: cover.indexOf("http") === 0 ? cover : BASE + cover,
      });
    }
    const q = query.toLowerCase();
    const filtered = items.filter((it) => it.name.toLowerCase().indexOf(q) >= 0);
    return { ok: true, data: { items: filtered, total: filtered.length } };
  }
  return { ok: true, data: { items: [], total: 0 } };
}

// ─── detail ────────────────────────────────────────────────────────────────
// Same /en/{slug} page; the RSC object framed by "slug":"{slug}" carries the
// metadata; JSON-LD description is the synopsis fallback.
function get_manga_detail(p) {
  const id = p && typeof p.id === "string" ? p.id : "";
  if (id === "") return { ok: true, data: {} };
  const html = get_body(BASE + "/en/" + host.url_encode(id));
  if (html === "") return { ok: true, data: { id: id } }; // bare id: retried next call
  const manga = find_object(rsc_chunks(html), '"slug":"' + id + '"');
  if (!manga) return { ok: true, data: { id: id } };
  let synopsis = manga.synopsis || "";
  if (!synopsis) {
    const ld = host.re2.find('"description":"([^"]{10,})","image"', html);
    if (ld.length > 1) synopsis = ld[1];
  }
  const cover = manga.coverImageUrl || "";
  return {
    ok: true,
    data: {
      id: manga.slug || id,
      name: manga.title || "",
      cover: cover ? (cover.indexOf("http") === 0 ? cover : BASE + cover) : "",
      status: host.text.normalize_status(manga.status || ""),
      tags: manga.genres || [],
      release_at: 0,
      description: host.text.strip_html(synopsis),
      authors: String(manga.author || "")
        .split(",")
        .map((s) => host.text.trim(s))
        .filter((s) => s !== ""),
      artists: [],
      url: BASE + "/en/" + id,
    },
  };
}

// ─── chapters ──────────────────────────────────────────────────────────────
// The chapters array sits in the /en/{slug} RSC stream, possibly across chunk
// boundaries; the concatenation is walked for a balanced "chapters": [.
// Ascending order = cppIsekai reading-order convention (host re-sorts too).
function get_chapter_list(p) {
  const id = p && typeof p.id === "string" ? p.id : "";
  const out = [];
  if (id !== "") {
    const html = get_body(BASE + "/en/" + host.url_encode(id));
    if (html !== "") {
      const arr = array_after(rsc_chunks(html).join(""), '"chapters":');
      if (Array.isArray(arr)) {
        for (const ch of arr) {
          const num = Number(ch && ch.chapterNumber);
          if (!isFinite(num)) continue;
          const t = Date.parse((ch && ch.createdAt) || "") / 1000;
          out.push({
            id: id + ":chapter-" + ch.chapterNumber,
            number: num,
            title: (ch && ch.chapterTitle) || "Chapter " + ch.chapterNumber,
            release_at: isFinite(t) && t > 0 ? Math.floor(t) : 0,
            pages: 0,
          });
        }
        out.sort((a, b) => a.number - b.number);
      }
    }
  }
  return { ok: true, data: out };
}

// ─── pages ─────────────────────────────────────────────────────────────────
// Reader page /en/{slug}-en-chapter-{n}; "pageImageUrls" is a plain array.
function get_page_list(p) {
  const id = p && typeof p.id === "string" ? p.id : "";
  if (id === "" || id.indexOf(":chapter-") < 0) return { ok: true, data: [] };
    const parts = id.split(":chapter-");
  const html = get_body(
    BASE + "/en/" + host.url_encode(parts[0]) + "-en-chapter-" + parts[1]
  );
  if (html === "") return { ok: true, data: [] };
  const urls = array_after(rsc_chunks(html).join(""), '"pageImageUrls":');
  return { ok: true, data: Array.isArray(urls) ? urls : [] };
}

// ─── genres ────────────────────────────────────────────────────────────────
// Catalog /en/manga renders genre filter pills as <button> labels; sort and
// filter controls are skipped; name doubles as slug (parity with original).
function get_genres() {
  const html = get_body(BASE + "/en/manga");
  const genres = [];
  if (html === "") return { ok: true, data: genres };
  const seen = {};
  const skip = {
    "latest update": 1, "a-z": 1, "most chapters": 1, "rating": 1, "all": 1,
    "ongoing": 1, "completed": 1, "hiatus": 1,
  };
  for (const m of host.re2.find_all(
    "<button[^>]*>([A-Za-z][A-Za-z &-]{1,18})</button>",
    html
  )) {
    const name = host.text.trim(m[1]);
    if (!name || skip[name] || seen[name]) continue;
    seen[name] = 1;
    genres.push(name);
  }
  return { ok: true, data: genres };
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
      return get_page_list(p);
    case "genres":
      return get_genres();
    default:
      return { ok: false, error: "unknown method " + ((req && req.method) || "") };
  }
}
