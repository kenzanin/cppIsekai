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
module.exports = { BASE, get_body, rsc_chunks, find_object, array_after };
