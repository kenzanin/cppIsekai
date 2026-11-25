const { get_body, rsc_chunks, find_object, array_after, BASE } = require("common");
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
module.exports = { get_manga_detail };
