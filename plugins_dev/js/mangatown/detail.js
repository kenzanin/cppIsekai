const { get_body, grp, decode_id, normalize_status } = require("common");
function get_manga_detail(p) {
  const id = decode_id(p);
  if (id === "") return { ok: true, data: {} };
  const html = get_body("https://m.mangatown.com/manga/" + id + "/");
  if (html === "") return { ok: true, data: { id: id } }; // bare id: retried next call
  const title = host.text.trim(host.text.unescape(grp(host.re2.find('class="title">([^<]+)<', html), 1)));
  const cover = host.text.unescape(grp(host.re2.find('<img src="([^"]+)"[^>]*class="detail-cover"', html), 1));
  const author = host.text.strip_html(grp(host.re2.find('Author\\(s\\):\\s*</span>(.*?)</p>', html), 1));
  const status = normalize_status(grp(host.re2.find('Status:\\s*</span>\\s*([^<]+)', html), 1));
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
module.exports = { get_manga_detail };
