const { get_body, grp, decode_id, normalize_status } = require("common");
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
module.exports = { search_manga };
