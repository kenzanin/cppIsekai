const { get_body, rsc_chunks, find_object, array_after, BASE } = require("common");
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
module.exports = { get_page_list };
