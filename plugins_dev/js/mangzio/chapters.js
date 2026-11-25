const { get_body, rsc_chunks, find_object, array_after, BASE } = require("common");
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
module.exports = { get_chapter_list };
