const { get_body, rsc_chunks, find_object, array_after, BASE } = require("common");
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
module.exports = { search_manga };
